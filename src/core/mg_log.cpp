//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
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

#include "mg/core/mg_log.h"

#include "mg/mg_defs.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_text_file_io.h"

#include <ctime>
#include <filesystem>
#include <iostream>

#include <fmt/format.h>
#include <fmt/time.h>

namespace Mg {

namespace fs = std::filesystem;

struct LogData {
    LogData(std::string_view file_path_,
            Log::Prio        console_verbosity_,
            Log::Prio        log_file_verbosity_)
        : console_verbosity(console_verbosity_)
        , log_file_verbosity(log_file_verbosity_)
        , file_path(file_path_)
    {}

    Log::Prio console_verbosity  = Log::Prio::Verbose;
    Log::Prio log_file_verbosity = Log::Prio::Verbose;

    std::string        file_path;
    Opt<std::ofstream> writer;
};

Log::Log(std::string_view file_path, Prio console_verbosity, Prio log_file_verbosity)
    : PimplMixin(file_path, console_verbosity, log_file_verbosity)
{
    data().writer = io::make_output_filestream(file_path, true);

    if (!data().writer) {
        std::cerr << "[ERROR]: "
                  << "Failed to open log file '" << file_path << "'.";
        return;
    }

    time_t log_open_time = time(nullptr);
    tm&    t             = *localtime(&log_open_time);

    *data().writer << fmt::format("Log started at {0:%F}, {0:%T}\n", t);
}

/** Set verbosity for console output */
void Log::set_console_verbosity(Prio prio)
{
    data().console_verbosity = prio;
}

/** Set verbosity for log file output */
void Log::set_file_verbosity(Prio prio)
{
    data().log_file_verbosity = prio;
}

Log::GetVerbosityReturn Log::get_verbosity() const
{
    return { data().console_verbosity, data().log_file_verbosity };
}

void Log::flush()
{
    if (data().writer) { data().writer->flush(); }
}

std::string_view Log::file_path() const
{
    return data().file_path;
}

void Log::output(Prio prio, std::string_view str)
{
    // Prepend message type
    const char* prefix;

    switch (prio) {
    case Prio::Error:
        prefix = "[ERROR]";
        break;
    case Prio::Warning:
        prefix = "[WARNING]";
        break;
    case Prio::Message:
        prefix = "[MESSAGE]";
        break;
    case Prio::Verbose:
        prefix = "[INFO]";
        break;
    default:
        MG_ASSERT_DEBUG(false);
        prefix = "";
    }

    std::string message = fmt::format("{} {}", prefix, str);

    if (prio <= data().console_verbosity) {
        // MSVC has terrible iostream performance on cout, so use puts()
        // (Apparently cout is unbuffered on MSVC but C functions like
        // puts() and printf() have their own internal buffering.)
        puts(message.c_str());
    }

    if (data().writer && prio <= data().log_file_verbosity) {
        // Write timestamp to log file
        time_t msg_time = time(nullptr);
        tm&    t        = *localtime(&msg_time);

        data().writer.value() << fmt::format("{:%T}: {}\n", t, message);

        // If message was a warning or error, make sure it's written to file
        // immediately (perhaps a crash is imminent!)
        if (prio <= Prio::Warning) { data().writer->flush(); }
    }
}

//--------------------------------------------------------------------------------------------------

// Nifty counter initialisation for engine log
static size_t                              nifty_counter;
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

detail::LogInitialiser::LogInitialiser()
{
    if (nifty_counter++ == 0) { new (&g_log) Log{ defs::k_engine_log_file }; }
}

detail::LogInitialiser::~LogInitialiser()
{
    if (--nifty_counter == 0) { (&g_log)->~Log(); }
}

void write_crash_log(Log& log)
{
    time_t crash_time = time(nullptr);
    tm&    t          = *localtime(&crash_time);

    auto out_directory_name = fmt::format("crashlog_{:%F}_{:%T}", t);

    auto fp            = log.file_path();
    auto log_path      = fs::u8path(fp.begin(), fp.end());
    auto log_directory = log_path.parent_path();
    auto log_filename  = log_path.filename();

    auto out_directory = log_directory /= fs::u8path(out_directory_name);
    fs::create_directory(out_directory);

    auto outpath = out_directory /= log_filename;

    log.write_message(fmt::format("Saving crash log '{}'", fs::absolute(outpath).u8string()));
    log.flush();

    fs::copy(log_path, outpath);
}

} // namespace Mg
