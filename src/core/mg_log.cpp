//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_log.h"

#include "mg/mg_defs.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_file_io.h"
#include "mg/utils/mg_optional.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
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

class LogImpl {
public:
    LogImpl(std::string_view file_path_,
            Log::Prio console_verbosity_,
            Log::Prio log_file_verbosity_)
        : console_verbosity(console_verbosity_)
        , log_file_verbosity(log_file_verbosity_)
        , file_path(file_path_)
        , m_writer(io::make_output_filestream(file_path, true, io::Mode::text))
    {
        if (!m_writer) {
            std::cerr << "[ERROR]: "
                      << "Failed to open log file '" << file_path << "'.";
            return;
        }

        const time_t log_open_time = time(nullptr);
        const tm& t = *localtime(&log_open_time);

        m_writer.value() << fmt::format("Log started at {0:%F}, {0:%T}\n", t);

        m_log_thread = std::thread([this] { run_loop(); });
    }

    ~LogImpl()
    {
        m_is_exiting = true;
        m_queue_condvar.notify_one();
        m_log_thread.join();
    }

    MG_MAKE_NON_COPYABLE(LogImpl);
    MG_MAKE_NON_MOVABLE(LogImpl);

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
        std::scoped_lock lock{ m_write_mutex };
        if (m_writer.has_value()) {
            m_writer->flush();
        }
    }

    Log::Prio console_verbosity = Log::Prio::Verbose;
    Log::Prio log_file_verbosity = Log::Prio::Verbose;

    std::string file_path;

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
            if (m_queued_messages.empty()) {
                m_queue_condvar.wait(queue_lock);
            }
        }
    }

    void write_out(LogItem&& item)
    {
        std::string formatted_message = format_message(item, true);

        std::cout << formatted_message << '\n';

        if (item.prio <= log_file_verbosity && m_writer) {
            m_writer.value() << formatted_message << '\n';
        }
    }

    Opt<std::ofstream> m_writer;
    std::thread m_log_thread;
    std::mutex m_queue_mutex;
    std::mutex m_write_mutex;
    std::condition_variable m_queue_condvar;
    std::deque<LogItem> m_queued_messages;
    bool m_is_exiting = false;
};

Log::Log(std::string_view file_path, Prio console_verbosity, Prio log_file_verbosity)
    : PImplMixin(file_path, console_verbosity, log_file_verbosity)
{}

Log::~Log() = default;

/** Set verbosity for console output */
void Log::set_console_verbosity(Prio prio) noexcept
{
    impl().console_verbosity = prio;
}

/** Set verbosity for log file output */
void Log::set_file_verbosity(Prio prio) noexcept
{
    impl().log_file_verbosity = prio;
}

Log::GetVerbosityReturn Log::get_verbosity() const noexcept
{
    return { impl().console_verbosity, impl().log_file_verbosity };
}

void Log::write(Prio prio, std::string msg)
{
    impl().enqueue({ prio, std::move(msg) });
}

void Log::flush()
{
    impl().flush();
}

std::string_view Log::file_path() const noexcept
{
    return impl().file_path;
}

//--------------------------------------------------------------------------------------------------

// Nifty counter initialisation for engine log
static size_t nifty_counter;
static std::aligned_storage_t<sizeof(Log)> log_buf;

// GCC 7 helpfully warns against strict aliasing violations when using reinterpret_cast; but, as I
// understand it, this case is actually well-defined, since g_log will only be accessed after
// placement-new in LogInitialiser's constructor.
#ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

Log& g_log = reinterpret_cast<Log&>(log_buf); // NOLINT

#ifdef __GNUC__
#    pragma GCC diagnostic pop
#endif

detail::LogInitialiser::LogInitialiser() noexcept
{
    if (nifty_counter++ == 0) {
        new (&g_log) Log{ defs::k_engine_log_file };
    }
}

detail::LogInitialiser::~LogInitialiser()
{
    if (--nifty_counter == 0) {
        (&g_log)->~Log();
    }
}

void write_crash_log(Log& log)
{
    const time_t crash_time = time(nullptr);
    const tm& t = *localtime(&crash_time);

    auto out_directory_name = fmt::format("crashlog_{:%F}_{:%T}", t);

    const auto fp = log.file_path();
    const auto log_path = fs::u8path(fp.begin(), fp.end());
    const auto log_directory = log_path.parent_path();
    const auto log_filename = log_path.filename();

    const auto out_directory = log_directory / fs::u8path(out_directory_name);
    fs::create_directory(out_directory);

    const auto outpath = out_directory / log_filename;

    log.write_message(
        fmt::format("Saving crash log '{}'", fs::absolute(outpath).generic_u8string()));
    log.flush();

    fs::copy(log_path, outpath);
}

} // namespace Mg
