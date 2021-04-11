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

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <fmt/core.h>

#include <string>
#include <string_view>
#include <utility>

#if MG_ENABLE_DEBUG_LOGGING
#    define MG_LOG_DEBUG(...) ::Mg::log.write(::Mg::Log::Prio::Debug, __VA_ARGS__)
#else
#    define MG_LOG_DEBUG(...) static_cast<void>(0);
#endif

namespace Mg {

class LogImpl;

/** Outputs messages with different priorities to console and file. */
class Log : PImplMixin<LogImpl> {
public:
    /** Message priorities, decides which messages should be included in file
     * and console output.
     */
    enum class Prio { Error, Warning, Message, Verbose, Debug };

    explicit Log(std::string_view file_path,
                 Prio console_verbosity = Prio::Debug,
                 Prio log_file_verbosity = Prio::Debug);
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

    template<typename T, typename... Ts>
    void write(Prio prio, std::string_view msg, T&& arg, Ts&&... args)
    {
        write_impl(prio, fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Writes a message with priority Error */
    void error(std::string msg) { write_impl(Prio::Error, std::move(msg)); }

    /** Writes a message with priority Warning */
    void warning(std::string msg) { write_impl(Prio::Warning, std::move(msg)); }

    /** Writes a message with priority Message */
    void message(std::string msg) { write_impl(Prio::Message, std::move(msg)); }

    /** Writes a message with priority Verbose */
    void verbose(std::string msg) { write_impl(Prio::Verbose, std::move(msg)); }

    /** Formats and writes a message with priority Error */
    template<typename T, typename... Ts> void error(std::string_view msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Error, fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Formats and writes a message with priority Warning */
    template<typename T, typename... Ts> void warning(std::string_view msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Warning,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Formats and writes a message with priority Message */
    template<typename T, typename... Ts> void message(std::string_view msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Message,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Formats and writes a message with priority Verbose */
    template<typename T, typename... Ts> void verbose(std::string_view msg, T&& arg, Ts&&... args)
    {
        write_impl(Prio::Verbose,
                   fmt::format(msg, std::forward<T>(arg), std::forward<Ts>(args)...));
    }

    /** Flush the log, writing to file immediately. */
    void flush();

    /** Get path to log output file. */
    std::string_view file_path() const noexcept;

private:
    /** Writes message with priority prio. */
    void write_impl(Prio prio, std::string msg);
};

//--------------------------------------------------------------------------------------------------

// Nifty counter lifetime management of log instance
namespace detail {
struct LogInitialiser {
    LogInitialiser() noexcept;
    MG_MAKE_NON_COPYABLE(LogInitialiser);
    MG_MAKE_NON_MOVABLE(LogInitialiser);
    ~LogInitialiser();
};

static LogInitialiser log_initialiser;
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
