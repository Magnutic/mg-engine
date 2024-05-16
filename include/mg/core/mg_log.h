//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_log.h
 * Log class, takes messages of different priorities and outputs to console and
 * log file.
 */

#pragma once

#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"

#include <fmt/core.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if MG_ENABLE_DEBUG_LOGGING
#    define MG_LOG_DEBUG(...) ::Mg::log.write(::Mg::Log::Prio::Debug, __VA_ARGS__)
#else
#    define MG_LOG_DEBUG(...) static_cast<void>(0);
#endif

namespace Mg {

/** Outputs messages with different priorities to console and file. */
class Log {
public:
    /** Message priorities, decides which messages should be included in file
     * and console output.
     */
    enum class Prio { Error, Warning, Message, Verbose, Debug };

    explicit Log(std::string_view file_path,
                 Prio console_verbosity = Prio::Debug,
                 Prio log_file_verbosity = Prio::Debug,
                 size_t num_history_lines = 1000);
    ~Log();

    MG_MAKE_NON_COPYABLE(Log);
    MG_MAKE_NON_MOVABLE(Log);

    /** Set verbosity for console output */
    void set_console_verbosity(Prio prio) noexcept;

    /** Set verbosity for log file output */
    void set_file_verbosity(Prio prio) noexcept;

    struct GetVerbosityReturn {
        Prio console_verbosity;
        Prio log_file_verbosity;
    };
    /** Get verbosity levels. */
    GetVerbosityReturn get_verbosity() const noexcept;

    void write(Prio prio, std::string msg) { write_impl(prio, std::move(msg)); }

    void write_once(Prio prio, std::string msg, float duplicate_message_timeout_seconds)
    {
        write_impl(prio, std::move(msg), duplicate_message_timeout_seconds);
    }

    template<typename T, typename... Ts>
    void write(Prio prio, fmt::format_string<T, Ts...> msg, T&& arg, Ts&&... args)
    {
        write_impl(prio, fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    template<typename T, typename... Ts>
    void write_once(Prio prio,
                    fmt::format_string<T, Ts...> msg,
                    T&& arg,
                    Ts&&... args,
                    float duplicate_message_timeout_seconds)
    {
        write_impl(prio,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...),
                   duplicate_message_timeout_seconds);
    }

    /** Writes a message with priority Error */
    void error(std::string msg) { write_impl(Prio::Error, std::move(msg)); }

    /** Writes a message with priority Error, but at most once within the timeout.
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     */
    void error_once(std::string msg, float duplicate_message_timeout_seconds)
    {
        write_impl(Prio::Error, std::move(msg), duplicate_message_timeout_seconds);
    }

    /** Writes a message with priority Warning */
    void warning(std::string msg) { write_impl(Prio::Warning, std::move(msg)); }

    /** Writes a message with priority Warning, but at most once within the timeout
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     . */
    void warning_once(std::string msg, float duplicate_message_timeout_seconds)
    {
        write_impl(Prio::Warning, std::move(msg), duplicate_message_timeout_seconds);
    }

    /** Writes a message with priority Message */
    void message(std::string msg) { write_impl(Prio::Message, std::move(msg)); }

    /** Writes a message with priority Message, but at most once within the timeout.
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     */
    void message_once(std::string msg, float duplicate_message_timeout_seconds)
    {
        write_impl(Prio::Message, std::move(msg), duplicate_message_timeout_seconds);
    }

    /** Writes a message with priority Verbose */
    void verbose(std::string msg) { write_impl(Prio::Verbose, std::move(msg)); }

    /** Writes a message with priority Verbose, but at most once within the timeout.
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     */
    void verbose_once(std::string msg, float duplicate_message_timeout_seconds)
    {
        write_impl(Prio::Verbose, std::move(msg), duplicate_message_timeout_seconds);
    }

    /** Formats and writes a message with priority Error */
    template<typename T, typename... Ts>
    void error(fmt::format_string<T, Ts...> msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Error, fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Formats and writes a message with priority Error, but at most once within the timeout.
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     */
    template<typename T, typename... Ts>
    void error_once(float duplicate_message_timeout_seconds,
                    fmt::format_string<T, Ts...> msg,
                    T&& arg,
                    Ts&&... args)
    {
        write_impl(Prio::Error,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...),
                   duplicate_message_timeout_seconds);
    }

    /** Formats and writes a message with priority Warning */
    template<typename T, typename... Ts>
    void warning(fmt::format_string<T, Ts...> msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Warning,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Formats and writes a message with priority Warning, but at most once within the timeout.
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     */
    template<typename T, typename... Ts>
    void warning_once(float duplicate_message_timeout_seconds,
                      fmt::format_string<T, Ts...> msg,
                      T&& arg,
                      Ts&&... args)
    {
        write_impl(Prio::Warning,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...),
                   duplicate_message_timeout_seconds);
    }

    /** Formats and writes a message with priority Message */
    template<typename T, typename... Ts>
    void message(fmt::format_string<T, Ts...> msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Message,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Formats and writes a message with priority Message, but at most once within the timeout.
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     */
    template<typename T, typename... Ts>
    void message_once(float duplicate_message_timeout_seconds,
                      fmt::format_string<T, Ts...> msg,
                      T&& arg,
                      Ts&&... args)
    {
        write_impl(Prio::Message,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...),
                   duplicate_message_timeout_seconds);
    }

    /** Formats and writes a message with priority Verbose */
    template<typename T, typename... Ts>
    void verbose(fmt::format_string<T, Ts...> msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Verbose,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Formats and writes a message with priority Verbose, but at most once within the timeout.
     * Note that there is some performance overhead to this, so prefer to avoid calling log too
     * often.
     */
    template<typename T, typename... Ts>
    void verbose_once(float duplicate_message_timeout_seconds,
                      fmt::format_string<T, Ts...> msg,
                      T&& arg,
                      Ts&&... args)
    {
        write_impl(Prio::Verbose,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...),
                   duplicate_message_timeout_seconds);
    }

    /** Flush the log, writing to file immediately. */
    void flush();

    /** Get path to log output file. */
    std::string_view file_path() const noexcept;

    /** Get a copy of the log history. */
    std::vector<std::string> get_history();

private:
    /** Writes message with priority prio. */
    void write_impl(Prio prio, std::string msg, float duplicate_message_timeout_seconds = 0.0);

    class Impl;
    ImplPtr<Impl> m_impl;
};

//--------------------------------------------------------------------------------------------------

// Nifty counter lifetime management of log instance
namespace detail {
struct LogInitializer {
    LogInitializer() noexcept;
    MG_MAKE_NON_COPYABLE(LogInitializer);
    MG_MAKE_NON_MOVABLE(LogInitializer);
    ~LogInitializer();
};

static LogInitializer log_initializer;
} // namespace detail

/** Mg Engine main log. */
extern Log& log;

/** Write a copy of log to a crashlog directory.
 *
 * Creates a subdirectory in log's output directory with the name
 * 'crashlog_<date>_<time>' and writes a copy of the log there.
 */
void write_crash_log();

} // namespace Mg
