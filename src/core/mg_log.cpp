//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_log.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/mg_defs.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_hash_fnv1a.h"
#include "mg/utils/mg_u8string_casts.h"

#include <chrono>
#include <fmt/chrono.h>

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

namespace Mg {

using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

struct LogItem {
    Log::Prio prio;
    std::string message;
    std::chrono::milliseconds duplicate_message_timeout;
};

constexpr const char* get_prefix(Log::Prio prio)
{
    switch (prio) {
    case Log::Prio::Error:
        return "[ERROR]";
        break;
    case Log::Prio::Warning:
        return "[WARNING]";
        break;
    case Log::Prio::Message:
        return "[MESSAGE]";
        break;
    case Log::Prio::Verbose:
        return "[INFO]";
        break;
    case Log::Prio::Debug:
        return "[DEBUG]";
    default:
        MG_ASSERT(false);
    }
}

auto now_localtime()
{
    return fmt::localtime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::string format_message(const LogItem& item)
{
    return fmt::format("{:%T}: {} {}", now_localtime(), get_prefix(item.prio), item.message);
}

} // namespace

/** Keeps a fixed-size history of most recent log lines. */
class LogHistory {
public:
    explicit LogHistory(const size_t num_history_lines) : m_num_history_lines{ num_history_lines }
    {
        m_history.reserve(num_history_lines);
    }

    void add(std::string&& message)
    {
        const auto history_index = m_history.empty()
                                       ? 0
                                       : (m_last_history_index + 1) % m_num_history_lines;
        if (history_index == m_history.size()) {
            m_history.emplace_back();
        }

        m_history[history_index] = message;
        m_last_history_index = history_index;
    }

    std::vector<std::string> get() const
    {
        std::vector<std::string> result;
        result.resize(m_history.size());

        for (size_t i = 0; i < m_history.size(); ++i) {
            const size_t source_index = (m_last_history_index + i) % m_history.size();
            result[i] = m_history[source_index];
        }

        return result;
    }

private:
    size_t m_num_history_lines;
    size_t m_last_history_index = 0u;
    std::vector<std::string> m_history;
};

/** Tracks log lines that we don't want to print repeatedly within too little time. */
class DuplicateMessageTracker {
public:
    bool can_print(const std::string& message, std::chrono::milliseconds timeout)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto hash = hash_fnv1a(message);

        if (auto it = m_items.find(hash); it != m_items.end()) {
            auto& item = it->second;
            const bool has_hash_collision = item.message != message;

            // Time out, or -- far less likely -- hash collision
            if (has_hash_collision || item.last_log_time + item.timeout < now) {
                item.last_log_time = now;
                item.timeout = timeout;
                if (has_hash_collision) {
                    item.message = message;
                }
                return true;
            }

            return false;
        }

        m_items.insert({ hash, { .last_log_time = now, .timeout = timeout, .message = message } });
        return true;
    }

private:
    struct Item {
        std::chrono::steady_clock::time_point last_log_time;
        std::chrono::milliseconds timeout = 1000ms;
        std::string message;
    };

    FlatMap<uint32_t, Item> m_items;
};

class Log::Impl {
public:
    Impl(std::string_view file_path_,
         Log::Prio console_verbosity_,
         Log::Prio log_file_verbosity_,
         const size_t num_history_lines)
        : console_verbosity(console_verbosity_)
        , log_file_verbosity(log_file_verbosity_)
        , file_path(file_path_)
        , history(num_history_lines)
        , m_writer(fs::path(cast_as_u8_unchecked(file_path_)), std::ios::trunc)
    {
        if (!m_writer) {
            std::cerr << "[ERROR]: "
                      << "Failed to open log file '" << file_path << "'.";
            return;
        }

        m_writer << fmt::format("Log started at {}\n", now_localtime());

        m_log_thread = std::thread([this] { run_loop(); });
    }

    ~Impl()
    {
        m_is_exiting = true;
        m_queue_condvar.notify_one();
        m_log_thread.join();
    }

    MG_MAKE_NON_COPYABLE(Impl);
    MG_MAKE_NON_MOVABLE(Impl);

    void enqueue(LogItem&& log_item)
    {
        if (m_is_exiting) {
            return;
        }
        {
            std::scoped_lock lock{ m_queue_mutex };
            m_queued_messages.push_front(std::move(log_item));
        }
        m_queue_condvar.notify_one();
    }

    void flush()
    {
        // Try to wait for writer thread to finish writing pending messages.
        // This may fail, in case other threads keep adding new messages.
        bool pending_messages_written = false;
        int remaining_wait_iterations = 10;
        constexpr auto wait_time_per_iteration = std::chrono::milliseconds(10);

        while (!pending_messages_written && remaining_wait_iterations-- > 0) {
            {
                std::scoped_lock lock{ m_queue_mutex };
                pending_messages_written = m_queued_messages.empty();
            }

            if (!pending_messages_written) {
                std::this_thread::sleep_for(wait_time_per_iteration);
            }
        }

        std::scoped_lock lock{ m_write_mutex };
        if (m_writer) {
            m_writer.flush();
        }
    }

    Log::Prio console_verbosity = Log::Prio::Verbose;
    Log::Prio log_file_verbosity = Log::Prio::Verbose;

    std::string file_path;
    LogHistory history;
    DuplicateMessageTracker duplicate_message_tracker;

private:
    void run_loop()
    {
        std::vector<LogItem> to_write;

        while (!m_is_exiting) {
            {
                std::scoped_lock queue_lock{ m_queue_mutex };
                std::move(m_queued_messages.begin(),
                          m_queued_messages.end(),
                          std::back_inserter(to_write));
                m_queued_messages.clear();
            }

            {
                std::scoped_lock write_lock{ m_write_mutex };
                while (!to_write.empty()) {
                    write_out(std::move(to_write.back()));
                    to_write.pop_back();
                }
            }

            std::unique_lock queue_lock{ m_queue_mutex };
            if (m_queued_messages.empty() && !m_is_exiting) {
                m_queue_condvar.wait(queue_lock);
            }
        }
    }

    void write_out(LogItem&& item)
    {
        // Drop duplicate messages that appear within the requested timeout.
        if (item.duplicate_message_timeout > 0ms &&
            !duplicate_message_tracker.can_print(item.message, item.duplicate_message_timeout)) {
            return;
        }

        std::string formatted_message = format_message(item);

        // Write to terminal.
        std::cout << formatted_message << '\n';

        // Write to file.
        if (item.prio <= log_file_verbosity && m_writer) {
            m_writer << formatted_message << '\n';
        }

        // Keep in history.
        history.add(std::move(formatted_message));
    }

    std::ofstream m_writer;
    std::thread m_log_thread;
    std::mutex m_queue_mutex;
    std::mutex m_write_mutex;
    std::condition_variable m_queue_condvar;
    std::deque<LogItem> m_queued_messages;
    std::atomic_bool m_is_exiting = false;
};

Log::Log(std::string_view file_path,
         Prio console_verbosity,
         Prio log_file_verbosity,
         const size_t num_history_lines)
    : m_impl(file_path, console_verbosity, log_file_verbosity, num_history_lines)
{}

Log::~Log() = default;

/** Set verbosity for console output */
void Log::set_console_verbosity(Prio prio) noexcept
{
    m_impl->console_verbosity = prio;
}

/** Set verbosity for log file output */
void Log::set_file_verbosity(Prio prio) noexcept
{
    m_impl->log_file_verbosity = prio;
}

Log::GetVerbosityReturn Log::get_verbosity() const noexcept
{
    return { m_impl->console_verbosity, m_impl->log_file_verbosity };
}

void Log::flush()
{
    m_impl->flush();
}

std::string_view Log::file_path() const noexcept
{
    return m_impl->file_path;
}

std::vector<std::string> Log::get_history()
{
    return m_impl->history.get();
}

void Log::write_impl(Prio prio, std::string msg, float duplicate_message_timeout_seconds)
{
    const auto duplicate_message_timeout =
        std::chrono::milliseconds{ narrow_cast<int>(duplicate_message_timeout_seconds * 1000.0f) };

    m_impl->enqueue({
        .prio = prio,
        .message = std::move(msg),
        .duplicate_message_timeout = duplicate_message_timeout,
    });

    // Flush on error messages, since a crash could be imminent.
    if (prio == Prio::Error) {
        flush();
    }
}

//--------------------------------------------------------------------------------------------------

// Nifty counter initialization for engine log
static size_t nifty_counter;
static std::aligned_storage_t<sizeof(Log)> log_buf;

// GCC 7 helpfully warns against strict aliasing violations when using reinterpret_cast; but, as I
// understand it, this case is actually well-defined, since log will only be accessed after
// placement-new in LogInitializer's constructor.
#ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

Log& log = reinterpret_cast<Log&>(log_buf); // NOLINT

#ifdef __GNUC__
#    pragma GCC diagnostic pop
#endif

detail::LogInitializer::LogInitializer() noexcept
{
    if (nifty_counter++ == 0) {
        new (&log) Log{ defs::k_engine_log_file };
    }
}

detail::LogInitializer::~LogInitializer()
{
    if (--nifty_counter == 0) {
        (&log)->~Log();
    }
}

void write_crash_log()
{
    auto out_directory_name =
        cast_as_u8_unchecked(fmt::format("crashlog_{0:%F}_{0:%T}", now_localtime()));

    const auto fp = cast_as_u8_unchecked(log.file_path());
    const auto log_path = fs::path(fp);
    const auto log_directory = log_path.parent_path();
    const auto log_filename = log_path.filename();

    const auto out_directory = log_directory / fs::path(out_directory_name);
    fs::create_directory(out_directory);

    const auto outpath = out_directory / log_filename;

    log.message("Saving crash log '{}'", fs::absolute(outpath).string());
    log.flush();

    fs::copy(log_path, outpath);
}

} // namespace Mg
