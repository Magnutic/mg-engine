#include "mg_mesh_converter.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace std::literals;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace Mg {

namespace {
bool convert(const fs::path& filename)
{
    fs::path out_filename = filename;
    out_filename.replace_extension(".mgm");

    if (!convert_mesh(filename, out_filename)) {
        std::cerr << "Failed to convert file '" << filename.u8string() << "'." << std::endl;
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

// Converts meshes if they have been modified more
// recently than the .fbx with the same filename
void convert_modified_files(const std::vector<fs::path>& input_files,
                            const std::vector<fs::path>& existing_files)
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
            if (compare_file_modified_times(input_map[key], s) < 0) {
                input_map[key] = s;
            }
        }
        else {
            input_map.insert({ std::move(key), s });
        }
    }

    for (auto& [key, in_file] : input_map) {
        if (output_map.count(key) > 0) {
            if (compare_file_modified_times(output_map[key], in_file) > 0) {
                continue;
            }
        }

        convert(in_file);
    }
}

// Appends the paths to all files under directory (recursively) with the given extension to result.
void add_files_to_list(const fs::path& extension,
                       const fs::path& directory,
                       std::vector<fs::path>& result)
{
    fs::recursive_directory_iterator directory_iterator(
        directory, fs::directory_options::follow_directory_symlink);

    for (const fs::path& file : directory_iterator) {
        if (!fs::is_regular_file(file)) {
            continue;
        }
        if (file.extension() == extension) {
            result.push_back(file);
        }
    }
}

void auto_mesh_converter(const fs::path& directory,
                         const milliseconds poll_time,
                         const bool print_found_files)
{
    std::cout << "Scanning directory " << directory << " for mesh files to convert.\n";

    for (;;) {
        std::vector<fs::path> source_files;
        std::vector<fs::path> mgm_files;

        // TODO maybe: could probably use a lot of different formats here
        add_files_to_list(".fbx", directory, source_files);
        add_files_to_list(".dae", directory, source_files);
        add_files_to_list(".glb", directory, source_files);
        add_files_to_list(".mgm", directory, mgm_files);

        if (print_found_files) {
            std::cout << "\nSource files: \n";
            for (fs::path& s : source_files) {
                std::cout << s << '\n';
            }
            std::cout << "\nMGM files: \n";
            for (fs::path& s : mgm_files) {
                std::cout << s << '\n';
            }
        }

        convert_modified_files(source_files, mgm_files);
        std::this_thread::sleep_for(poll_time);
    }
}
} // namespace

} // namespace Mg

int main(int argc, char* argv[])
{
    std::cout << "mg_mesh_converter: converts fbx, dae, and glb meshes to Mg mesh format.\n";

    if (argc == 2) {
        return Mg::convert(argv[1]) ? 0 : 1;
    }

    const auto poll_time = 500ms;
    Mg::auto_mesh_converter(fs::current_path(), poll_time, false);
}
