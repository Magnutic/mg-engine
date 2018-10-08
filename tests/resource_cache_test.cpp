#include "catch.hpp"

#include <mg/core/mg_resource_cache.h>
#include <mg/resources/mg_text_resource.h>

TEST_CASE("ResourceCache test")
{
    constexpr size_t cache_size = 1024;

    constexpr static const char directory_name[] = "data/test-archive";
    constexpr static const char archive_name[]   = "data/test-archive.zip";

    Mg::ResourceCache cache(cache_size,
                            std::make_shared<Mg::BasicFileLoader>(directory_name),
                            std::make_shared<Mg::ZipFileLoader>(archive_name));

    SECTION("can_construct") {}

    SECTION("get_name")
    {
        auto has_loader_with_name = [&](std::string_view name) {
            for (auto&& p_loader : cache.file_loaders()) {
                if (p_loader->name() == name) return true;
            }
            return false;
        };

        REQUIRE(has_loader_with_name(directory_name));
        REQUIRE(has_loader_with_name(archive_name));
    }

    SECTION("finds_archive_content")
    {
        puts("Starting finds_archive_content test");

        REQUIRE(cache.file_exists("test-file-1.txt"));
        REQUIRE(cache.file_exists("test-file-3.txt"));
    }


    SECTION("finds_directory_content")
    {
        puts("Starting finds_directory_content test");

        REQUIRE(cache.file_exists("test-file-2.txt"));
        REQUIRE(cache.file_exists("test-file-3.txt"));
    }


    SECTION("find_subdirectory_content")
    {
        puts("Starting find_subdirectory_content test");

        REQUIRE(cache.file_exists("subdirectory/test-file-4.txt"));
        REQUIRE(cache.file_exists("subdirectory/test-file-5.txt"));
    }


    SECTION("load_file_from_directory")
    {
        puts("Starting load_file_from_directory test");

        cache.access_resource<Mg::TextResource>("test-file-2.txt");
        REQUIRE(cache.is_cached("test-file-2.txt"));
    }


    SECTION("load_file_from_archive")
    {
        puts("Starting load_file_from_archive test");

        cache.access_resource<Mg::TextResource>("test-file-1.txt");
        REQUIRE(cache.is_cached("test-file-1.txt"));
    }


    SECTION("get_file_contents")
    {
        puts("Starting get_file_contents test");

        auto res_access = cache.access_resource<Mg::TextResource>("test-file-2.txt");
        auto text       = res_access->text();
        REQUIRE(text == "test-file-2");
    }


    SECTION("remove_from_cache")
    {
        puts("Starting remove_from_cache test");

        {
            auto res_access = cache.access_resource<Mg::TextResource>("test-file-2.txt");
            auto text       = res_access->text();
            REQUIRE(!text.empty());
            REQUIRE(cache.is_cached("test-file-2.txt"));
        }
        cache.unload_unused();
        REQUIRE(!cache.is_cached("test-file-2.txt"));
    }


    SECTION("resource_access")
    {
        puts("Starting resource_access test");

        auto access0 = cache.access_resource<Mg::TextResource>("test-file-1.txt");
        auto text0   = access0->text();
        REQUIRE(text0 == "test-file-1");

        auto access1 = cache.access_resource<Mg::TextResource>("test-file-2.txt");
        auto text1   = access1->text();
        REQUIRE(text1 == "test-file-2");

        cache.access_resource<Mg::TextResource>("test-file-3.txt");
        cache.access_resource<Mg::TextResource>("subdirectory/test-file-4.txt");

        REQUIRE(cache.unload_unused());

        REQUIRE(cache.is_cached("test-file-1.txt"));
        REQUIRE(cache.is_cached("test-file-2.txt"));

        REQUIRE(!cache.is_cached("test-file-3.txt"));
        REQUIRE(cache.is_cached("subdirectory/test-file-4.txt"));

        REQUIRE(cache.unload_unused());

        REQUIRE(cache.is_cached("test-file-1.txt"));
        REQUIRE(cache.is_cached("test-file-2.txt"));

        REQUIRE(!cache.is_cached("test-file-3.txt"));
        REQUIRE(!cache.is_cached("subdirectory/test-file-4.txt"));

        REQUIRE(!cache.unload_unused());
        REQUIRE(!cache.unload_unused());

        REQUIRE(cache.is_cached("test-file-1.txt"));
        REQUIRE(cache.is_cached("test-file-2.txt"));
    }

    SECTION("scoped_resource_access")
    {
        puts("Starting scoped_resource_access test");

        {
            auto access0 = cache.access_resource<Mg::TextResource>("test-file-1.txt");
            REQUIRE(cache.is_cached("test-file-1.txt"));
            REQUIRE(!cache.unload_unused());
        }

        REQUIRE(cache.unload_unused());
        REQUIRE(!cache.is_cached("test-file-1.txt"));
    }
}
