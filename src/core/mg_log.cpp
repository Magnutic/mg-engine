//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_log.h"

#include "mg/mg_defs.h"
#include "mg/utils/mg_assert.h"

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

namespace fs = std::filesystem;

namespace {

struct LogItem {
    Log::Prio prio;
    std::string message;
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

std::string format_message(const LogItem& item, bool include_time_stamp)
{
    // Prepend message type
    std::string message = fmt::format("{} {}", get_prefix(item.prio), item.message);

    if (include_time_stamp) {
        const time_t msg_time = time(nullptr);
        const tm& t = *localtime(&msg_time);
        message = fmt::format("{:%T}: {}", t, message);
    }

    return message;
}

} // namespace

class Log::Impl {
public:
    Impl(std::string_view file_path_,
            Log::Prio console_verbosity_,
            Log::Prio log_file_verbosity_,
            const size_t num_history_lines_)
        : console_verbosity(console_verbosity_)
        , log_file_verbosity(log_file_verbosity_)
        , file_path(file_path_)
        , num_history_lines(num_history_lines_)
        , m_writer(fs::u8path(file_path_), std::ios::trunc)
    {
        if (!m_writer) {
            std::cerr << "[ERROR]: "
                      << "Failed to open log file '" << file_path << "'.";
            return;
        }

        history.reserve(num_history_lines);

        const time_t log_open_time = time(nullptr);
        const tm& t = *localtime(&log_open_time);

        m_writer << fmt::format("Log started at {0:%F}, {0:%T}\n", t);

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

    size_t num_history_lines;
    size_t last_history_index = 0u;
    std::vector<std::string> history;

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
        std::string formatted_message = format_message(item, true);

        // Write to terminal.
        std::cout << formatted_message << '\n';

        // Write to file.
        if (item.prio <= log_file_verbosity && m_writer) {
            m_writer << formatted_message << '\n';
        }

        // Keep in history.
        const auto history_index = history.empty() ? 0
                                                   : (last_history_index + 1) % num_history_lines;
        if (history_index == history.size()) {
            history.emplace_back();
        }
        history[history_index] = formatted_message;
        last_history_index = history_index;
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
    std::vector<std::string> result;
    result.resize(m_impl->history.size());

    for (size_t i = 0; i < m_impl->history.size(); ++i) {
        const size_t source_index = (m_impl->last_history_index + i) % m_impl->history.size();
        result[i] = m_impl->history[source_index];
    }

    return result;
}

void Log::write_impl(Prio prio, std::string msg)
{
    m_impl->enqueue({ prio, std::move(msg) });
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
    const time_t crash_time = time(nullptr);
    const tm& t = *localtime(&crash_time);

    auto out_directory_name = fmt::format("crashlog_{:%T}", t);

    const auto fp = log.file_path();
    const auto log_path = fs::u8path(fp.begin(), fp.end());
    const auto log_directory = log_path.parent_path();
    const auto log_filename = log_path.filename();

    const auto out_directory = log_directory / fs::u8path(out_directory_name);
    fs::create_directory(out_directory);

    const auto outpath = out_directory / log_filename;

    log.message("Saving crash log '{}'", fs::absolute(outpath).generic_u8string());
    log.flush();

    fs::copy(log_path, outpath);
}

} // namespace Mg
