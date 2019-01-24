//**************************************************************************************************
// Mg Engine
//-------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_log.h
 * Log class, takes messages of different priorities and outputs to console and
 * log file.
 */

#pragma once

#include <fstream>
#include <optional>
#include <string>

#include <mg/utils/mg_format_string.h>

namespace Mg {

/** Outputs messages with different priorities to console and file. */
class Log {
public:
    /** Message priorities, decides which messages should be included in file
     * and console output.
     */
    enum class Prio { Error, Warning, Message, Verbose };

    explicit Log(std::string_view file_path,
                 Prio             console_verbosity  = Prio::Verbose,
                 Prio             log_file_verbosity = Prio::Verbose);

    /** Set verbosity for console output */
    void set_console_verbosity(Prio prio) { m_console_verbosity = prio; }

    /** Set verbosity for log file output */
    void set_file_verbosity(Prio prio) { m_log_file_verbosity = prio; }

    /** Writes (type safe) printf-formatted message with priority prio */
    template<size_t N, typename... Args>
    void write(Prio prio, const char (&msg)[N], const Args&... args);

    /** Writes message with priority prio. */
    void write(Prio prio, std::string_view msg) { output(prio, std::string(msg)); }

    // Convenience functions:

    /** Writes a message with priority Error */
    template<size_t N, typename... Args> void write_error(const char (&msg)[N], const Args&... args)
    {
        write(Prio::Error, msg, args...);
    }

    /** Writes a message with priority Error */
    void write_error(std::string_view msg) { write(Prio::Error, msg); }

    /** Writes a message with priority Warning */
    template<size_t N, typename... Args>
    void write_warning(const char (&msg)[N], const Args&... args)
    {
        write(Prio::Warning, msg, args...);
    }

    /** Writes a message with priority Warning */
    void write_warning(std::string_view msg) { write(Prio::Warning, msg); }

    /** Writes a message with priority Message */
    template<size_t N, typename... Args>
    void write_message(const char (&msg)[N], const Args&... args)
    {
        write(Prio::Message, msg, args...);
    }

    /** Writes a message with priority Message */
    void write_message(std::string_view msg) { write(Prio::Message, msg); }

    /** Writes a message with priority Verbose */
    template<size_t N, typename... Args>
    void write_verbose(const char (&msg)[N], const Args&... args)
    {
        write(Prio::Verbose, msg, args...);
    }

    /** Writes a message with priority Verbose */
    void write_verbose(std::string_view msg) { write(Prio::Verbose, msg); }

    void flush();

    /** Get path to log output file. */
    std::string_view file_path() const { return m_file_path; }

private:
    Log(const Log& other) = delete;
    Log& operator=(const Log& other) = delete;

    void output(Prio prio, const std::string& str);

    Prio m_console_verbosity  = Prio::Verbose;
    Prio m_log_file_verbosity = Prio::Verbose;

    std::string                  m_file_path;
    std::optional<std::ofstream> m_writer;
};

template<size_t N, typename... Args>
void Log::write(Prio prio, const char (&msg)[N], const Args&... args)
{
    // Ignore messages with lower priority than current setting
    if (prio > m_console_verbosity && prio > m_log_file_verbosity) {
        return;
    }

    auto formatted_str = format_string(msg, args...);
    output(prio, formatted_str);
}

//--------------------------------------------------------------------------------------------------

// Nifty counter lifetime management of log instance
namespace detail {
struct LogInitialiser {
    LogInitialiser();
    ~LogInitialiser();
};

static LogInitialiser log_initialiser;
} // namespace detail

/** Mg Engine main log. */
extern Log& g_log;

/** Write a copy of log to a crashlog directory.
 *
 * Creates a subdirectory in log's output directory with the name
 * 'crashlog_<date>_<time>' and writes a copy of the log there.
 */
void write_crash_log(Log& log);

} // namespace Mg
