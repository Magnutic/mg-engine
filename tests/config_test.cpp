#include "catch.hpp"

#include <string>

#include <mg/core/mg_config.h>
#include <mg/utils/mg_gsl.h>

TEST_CASE("Config")
{
    std::string_view file = "data/test-config.cfg";
    Mg::Config config;

    config.set_default_value("var0", 10);
    config.set_default_value("int_value", 0);
    config.set_default_value("oddly_formatted_int_value_1", 0);
    config.set_default_value("oddly_formatted_int_value_2", 0);

    config.set_default_value("float_value", 0.0);
    config.set_default_value("string_value", "");
    config.set_default_value("string_value_sentence", "");

    config.set_default_value("bool_value_1", false);
    config.set_default_value("bool_value_2", false);

    SECTION("Default values")
    {
        REQUIRE(config.as<int>("var0") == 10);
        REQUIRE(config.as<int>("int_value") == 0);
        REQUIRE(config.as<int>("oddly_formatted_int_value_1") == 0);
        REQUIRE(config.as<int>("oddly_formatted_int_value_2") == 0);

        REQUIRE(config.as<float>("float_value") == 0.0);
        REQUIRE(config.as_string("string_value") == "");
        REQUIRE(config.as_string("string_value_sentence") == "");

        REQUIRE(config.as<bool>("bool_value_1") == false);
        REQUIRE(config.as<bool>("bool_value_2") == false);
    }

    SECTION("Reading from file")
    {
        config.read_from_file(file);

        REQUIRE(config.as<int>("var0") == 10);
        REQUIRE(config.as<int>("int_value") == 1);
        REQUIRE(config.as<int>("oddly_formatted_int_value_1") == 1);
        REQUIRE(config.as<int>("oddly_formatted_int_value_2") == 2);

        REQUIRE(config.as<float>("float_value") == -13.50f);
        REQUIRE(config.as_string("string_value") == "hello");
        REQUIRE(config.as_string("string_value_sentence") == "hello hello");

        REQUIRE(config.as<bool>("bool_value_1") == true);
        REQUIRE(config.as<bool>("bool_value_2") == false);
    }
} // TEST_CASE("Config")
