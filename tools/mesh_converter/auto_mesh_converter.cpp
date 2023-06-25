#include "mg_mesh_converter.h"

#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"
#include "mg/utils/mg_u8string_casts.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::literals;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace Mg {

namespace {
bool convert(const fs::path& filename, const bool debug_logging)
{
    fs::path out_filename = filename;
    out_filename.replace_extension(".mgm");

    if (!convert_mesh(filename, out_filename, debug_logging)) {
        std::cerr << "Failed to convert file '" << cast_u8_to_char(filename.u8string()) << "'."
                  << std::endl;
        return true;
    }

    return false;
}

int64_t compare_file_modified_times(const fs::path& file1, const fs::path& file2)
{
    std::error_code file1_error;
    std::error_code file2_error;
    const fs::file_time_type file1_time = fs::last_write_time(file1, file1_error);
    const fs::file_time_type file2_time = fs::last_write_time(file2, file2_error);

    if (file1_error) {
        std::cerr << "Warning: file '" << file1 << "' could not be read: " << file1_error.message()
                  << "\n";
        return 0;
    }

    if (file2_error) {
        std::cerr << "Warning: file '" << file2 << "' could not be read: " << file2_error.message()
                  << "\n";
        return 0;
    }

    const auto diff = file1_time - file2_time;
    const auto diff_in_seconds = duration_cast<seconds>(diff);
    return diff_in_seconds.count();
}

struct AutoConverterSettings {
    milliseconds poll_time;
    bool print_found_files = false;
    bool ignore_timestamps = false;
    bool repeat_forever = false;
    bool debug_logging = false;
};

// Converts meshes if they have been modified more
// recently than the input with the same filename
void convert_modified_files(const std::vector<fs::path>& input_files,
                            const std::vector<fs::path>& existing_files,
                            const AutoConverterSettings& settings)
{
    // Construct a map of files, keyed without extension, so we can
    // compare input and FBX files
    std::map<fs::path, fs::path> input_map;
    std::map<fs::path, fs::path> output_map;

    for (const fs::path& s : existing_files) {
        fs::path key = s;
        key.replace_extension("");
        output_map.insert({ std::move(key), s });
    }

    for (const fs::path& s : input_files) {
        fs::path key = s;
        key.replace_extension("");

        // If we already added a file with same name but different extension
        if (input_map.count(key) > 0) {
            // Replace if new file is more recently modified
            if (settings.ignore_timestamps || compare_file_modified_times(input_map[key], s) < 0) {
                input_map[key] = s;
            }
        }
        else {
            input_map.insert({ std::move(key), s });
        }
    }

    for (auto& [key, in_file] : input_map) {
        if (output_map.count(key) > 0) {
            if (!settings.ignore_timestamps &&
                compare_file_modified_times(output_map[key], in_file) > 0) {
                continue;
            }
        }

        convert(in_file, settings.debug_logging);
    }
}

// Appends the paths to all files under directory (recursively) with the given extension to result.
void add_files_to_list(const std::vector<std::string_view>& extensions,
                       const fs::path& directory,
                       std::vector<fs::path>& result)
{
    fs::recursive_directory_iterator directory_iterator(
        directory, fs::directory_options::follow_directory_symlink);

    auto has_sought_extension = [&extensions](const fs::path& filepath) {
        const auto file_extension =
            to_lower(cast_u8_to_char(filepath.extension().generic_u8string()));

        return any_of(extensions, [&](const std::string_view& extension) {
            return extension == file_extension;
        });
    };

    for (const fs::path& file : directory_iterator) {
        if (!fs::is_regular_file(file)) {
            continue;
        }

        if (has_sought_extension(file)) {
            result.push_back(file);
        }
    }
}

void auto_mesh_converter(const fs::path& directory, const AutoConverterSettings& settings)
{
    std::cout << "Scanning directory " << directory << " for mesh files to convert.\n";

    for (;;) {
        std::vector<fs::path> source_files;
        std::vector<fs::path> mgm_files;

        // TODO maybe: could probably use a lot more formats here
        add_files_to_list({ ".fbx", ".dae", ".glb", ".gltf" }, directory, source_files);
        add_files_to_list({ ".mgm" }, directory, mgm_files);

        if (settings.print_found_files) {
            std::cout << "\nSource files: \n";
            for (fs::path& s : source_files) {
                std::cout << s << '\n';
            }
            std::cout << "\nMGM files: \n";
            for (fs::path& s : mgm_files) {
                std::cout << s << '\n';
            }
        }

        convert_modified_files(source_files, mgm_files, settings);

        if (!settings.repeat_forever) {
            break;
        }

        std::this_thread::sleep_for(settings.poll_time);
    }
}
} // namespace

} // namespace Mg

int main(int argc, char* argv[])
{
    std::cout << "mesh_converter: converts fbx, dae, and glb meshes to Mg mesh format.\n";

    std::vector<std::string_view> args;
    args.reserve(size_t(argc));
    for (int i = 0; i < argc - 1; ++i) {
        args.emplace_back(argv[argc - i - 1]); // NOLINT
    }

    Mg::AutoConverterSettings settings = {};
    settings.poll_time = 1000ms;
    bool error = false;
    bool run_auto_converter = false;
    std::string_view file;

    std::vector<std::string_view> unrecognized_args;

    while (!args.empty()) {
        const auto arg = args.back();
        args.pop_back();

        if (arg == "--run-auto-converter") {
            run_auto_converter = true;
        }
        else if (arg == "--ignore-timestamps") {
            settings.ignore_timestamps = true;
        }
        else if (arg == "--repeat-forever") {
            settings.repeat_forever = true;
        }
        else if (arg == "--debug-logging") {
            settings.debug_logging = true;
        }
        else if (arg == "--file") {
            if (args.empty()) {
                std::cerr << "Expected file name after --file\n";
                error = true;
            }
            file = args.back();
            args.pop_back();
        }
        else {
            unrecognized_args.push_back(arg);
            error = true;
        }
    }

    for (const auto arg : unrecognized_args) {
        std::cerr << "Unrecognized argument: " << arg << '\n';
    }

    if (error || (file.empty() && !run_auto_converter)) {
        std::cerr << "Usage: mesh_converter <args>\nArguments:\n";

        std::cerr << "\t--file <filename> Convert the specified file\n";

        std::cerr << "\t--run-auto-converter Convert all model files for which there is not a "
                     "corresponding Mg mesh file with newer time stamp\n";

        std::cerr << "\t--ignore-timestamps When used in conjunction with --run-auto-converter, "
                     "will convert model files even if there is a corresponding Mg mesh file with "
                     "newer time stamp.\n";

        std::cerr << "\t--repeat-forever When used in conjunction with --run-auto-converter, "
                     "will repeat checking for model files to convert every second until the "
                     "application is cancelled.\n";

        return error ? 1 : 0;
    }

    if (!file.empty()) {
        return Mg::convert(file, settings.debug_logging) ? 0 : 1;
    }

    Mg::auto_mesh_converter(fs::current_path(), settings);
}
