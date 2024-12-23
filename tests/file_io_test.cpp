#include "catch.hpp"

#include <mg/utils/mg_file_io.h>

#include <fmt/core.h>

#include <cstdio>
#include <filesystem>

// Test uses POSIX functions, so skip on Windows.
#ifdef __linux__

#    include <stdlib.h>

TEST_CASE("filestream helpers")
{
    char buf[] = "/tmp/mgtestXXXXXX";
    char* directory = ::mkdtemp(buf);
    REQUIRE(directory);
    const auto file_path = std::string(directory) + "/test";

    SECTION("make_output_filestream, make_input_filestream text")
    {
        {
            // Verify cannot open non-existing
            auto [s, msg] = Mg::io::make_input_filestream(file_path, Mg::io::Mode::text);
            REQUIRE(!s.has_value());
        }
        {
            const bool overwrite = true;
            auto [s,
                  msg] = Mg::io::make_output_filestream(file_path, overwrite, Mg::io::Mode::text);
            REQUIRE(s.has_value());
            s.value() << "Test";
        }
        {
            // Append
            const bool overwrite = false;
            auto [s,
                  msg] = Mg::io::make_output_filestream(file_path, overwrite, Mg::io::Mode::text);
            REQUIRE(s.has_value());
            s.value() << "Test";
        }
        {
            // Verify appended
            auto [s, msg] = Mg::io::make_input_filestream(file_path, Mg::io::Mode::text);
            REQUIRE(s.has_value());
            REQUIRE(Mg::io::get_all_text(s.value()) == "TestTest");
        }
        {
            // Overwrite
            const bool overwrite = true;
            auto [s,
                  msg] = Mg::io::make_output_filestream(file_path, overwrite, Mg::io::Mode::text);
            REQUIRE(s.has_value());
            s.value() << "Overwritten";
        }
        {
            // Verify overwritten
            auto [s, msg] = Mg::io::make_input_filestream(file_path, Mg::io::Mode::text);
            REQUIRE(s.has_value());
            REQUIRE(Mg::io::get_all_text(s.value()) == "Overwritten");
        }
    }
}

#endif
