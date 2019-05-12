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

#include "mg/resources/mg_shader_resource.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/resource_cache/mg_resource_exceptions.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/resources/mg_text_resource.h"
#include "mg/resources/shader_parser/mg_shader_parser.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"

#include <fmt/core.h>

#include <glm/vec4.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace Mg {

namespace {

// Used delimit sections in assembled shader code.
static constexpr auto k_delimiter_comment = R"(
//==============================================================================
// Origin: {}
//==============================================================================

)";

// The search directories for a recursive #include statement, i.e. the same as
// include_directories but also the included file's directory.
std::vector<fs::path> include_dirs_for_file(std::vector<fs::path> include_directories,
                                            fs::path              included_file)
{
    const auto file_directory = included_file.parent_path();

    if (find(include_directories, file_directory) == include_directories.end()) {
        include_directories.push_back(file_directory);
    }

    return include_directories;
};

// Try to parse a line as an #include directive.
auto try_parse_line_as_include = [](std::string_view line) -> std::pair<bool, std::string> {
    const std::pair<bool, std::string> fail_value = { false, "" };

    SimpleInputStream stream(line);

    auto skip_whitespace = [&stream] {
        while (is_whitespace(stream.peek())) { stream.advance(); }
    };

    skip_whitespace();
    if (!stream.match('#')) { return fail_value; }
    skip_whitespace();
    if (!stream.match("include")) { return fail_value; }
    skip_whitespace();

    char terminator;

    if (stream.match('\"')) { terminator = '\"'; }
    else if (stream.match('<')) {
        terminator = '>';
    }
    else {
        return fail_value;
    }

    std::string include_path;

    while (stream.peek() != terminator) {
        if (stream.is_at_end()) { return fail_value; }
        include_path += stream.advance();
    }

    return { true, include_path };
};

// Helper for ShaderResource::load_resource. Assemble shader code by loading included code files.
std::string assemble_shader_code(std::vector<fs::path>       include_directories,
                                 std::string_view            code,
                                 const ResourceLoadingInput& input,
                                 fs::path                    source_file)
{
    auto get_code = [&input](fs::path file_path) -> std::pair<bool, std::string> {
        try {
            const auto          file_id     = Identifier::from_runtime_string(file_path.u8string());
            ResourceHandle      file_handle = input.load_dependency<TextResource>(file_id);
            ResourceAccessGuard include_access(file_handle);
            return { true, std::string(include_access->text()) };
        }
        catch (ResourceError&) {
            return { false, "" };
        }
    };

    auto get_include = [&](std::string_view include_file) -> std::pair<bool, std::string> {
        std::string result;

        for (auto include_directory : include_directories) {
            const fs::path file_path = include_directory / include_file;

            // Load include file as a dependency of this resource.
            auto [is_found, included_code] = get_code(file_path);

            if (!is_found) { continue; }

            // Add origin-tracking comment (helps debugging shader)
            result += fmt::format(k_delimiter_comment, file_path.u8string());

            // Recursively assemble loaded code.
            result += assemble_shader_code(include_dirs_for_file(include_directories, file_path),
                                           included_code,
                                           input,
                                           file_path);
            return { true, result };
        }

        return { false, "" };
    };

    // Read line-by-line, try to parse each line as an #include directive; if successful, include
    // the file, otherwise, insert the line as is.
    std::vector<std::string_view> lines = tokenise_string(code, "\n");
    std::string                   assembled_code;

    for (auto&& line : lines) {
        auto [line_is_include, include_path] = try_parse_line_as_include(line);

        if (!line_is_include) {
            assembled_code += line;
            assembled_code += '\n';

            continue;
        }

        auto [include_success, included_code] = get_include(include_path);
        if (!include_success) {
            g_log.write_error(fmt::format("Could not find '{}' (#include directive in '{}'.)",
                                          include_path,
                                          source_file.u8string()));
            throw RuntimeError{};
        }
        assembled_code += included_code;
    }

    return assembled_code;
}

} // namespace

LoadResourceResult ShaderResource::load_resource_impl(const ResourceLoadingInput& input)
{
    const std::string_view shader_resource_definition = input.resource_data_as_text();

    try {
        shader::ParseResult parse_result = shader::parse_shader(shader_resource_definition);
        m_parameters                     = std::move(parse_result.parameters);
        m_samplers                       = std::move(parse_result.samplers);
        m_options                        = std::move(parse_result.options);

        const auto filepath            = fs::path(resource_id().str_view());
        const auto include_directories = include_dirs_for_file({}, filepath);

        m_vertex_code   = assemble_shader_code(include_directories,
                                             parse_result.vertex_code,
                                             input,
                                             filepath);
        m_fragment_code = assemble_shader_code(include_directories,
                                               parse_result.fragment_code,
                                               input,
                                               filepath);

        // Sort parameters so that larger types come first (for the sake of alignment)
        sort(m_parameters, [](const shader::Parameter& l, const shader::Parameter& r) {
            return static_cast<int>(l.type) > static_cast<int>(r.type);
        });

        m_tags = parse_result.tags;
    }
    catch (const std::exception& e) {
        return LoadResourceResult::data_error(e.what());
    }

    return LoadResourceResult::success();
}

std::string ShaderResource::debug_print() const
{
    std::ostringstream oss;

    oss << "ShaderResource '" << resource_id().c_str() << "'\n";

    oss << "Options:\n";
    for (auto&& o : options()) { oss << "\t" << o.name.c_str() << "\n"; }

    oss << "Parameters:\n";
    for (auto&& p : parameters()) {
        oss << "\t" << shader::parameter_type_to_string(p.type) << " " << p.name.c_str() << "\n";
    }

    oss << "Samplers:\n";
    for (auto&& s : samplers()) {
        oss << "\t" << shader::sampler_type_to_string(s.type) << " " << s.name.c_str() << "\n";
    }

    return oss.str();
}

} // namespace Mg
