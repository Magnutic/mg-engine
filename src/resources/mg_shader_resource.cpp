//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_shader_resource.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/parser/mg_parser.h"
#include "mg/resource_cache/mg_resource_exceptions.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/resources/mg_text_resource.h"
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
constexpr auto k_delimiter_comment = R"(
//==============================================================================
// Origin: {}
//==============================================================================

)";

// The search directories for a recursive #include statement, i.e. the same as
// include_directories but also the included file's directory.
std::vector<fs::path> include_dirs_for_file(std::vector<fs::path> include_directories,
                                            const fs::path& included_file)
{
    const auto file_directory = included_file.parent_path();

    if (find(include_directories, file_directory) == include_directories.end()) {
        include_directories.push_back(file_directory);
    }

    return include_directories;
};

// Try to parse a line as an #include directive.
std::pair<bool, std::string> try_parse_line_as_include(std::string_view line)
{
    const std::pair<bool, std::string> fail_value = { false, "" };
    const auto peekmode = SimpleInputStream::PeekMode::return_eof_as_null_char;

    SimpleInputStream stream(line);

    const auto skip_whitespace = [&] {
        while (is_whitespace(stream.peek(peekmode))) {
            stream.advance();
        }
    };

    skip_whitespace();
    if (!stream.match('#', peekmode)) {
        return fail_value;
    }
    skip_whitespace();
    if (!stream.match("include", peekmode)) {
        return fail_value;
    }
    skip_whitespace();

    char terminator{};

    if (stream.match('\"', peekmode)) {
        terminator = '\"';
    }
    else if (stream.match('<', peekmode)) {
        terminator = '>';
    }
    else {
        return fail_value;
    }

    std::string include_path;

    while (stream.peek(peekmode) != terminator) {
        if (stream.is_at_end()) {
            return fail_value;
        }
        include_path += stream.advance();
    }

    return { true, include_path };
}

std::string assemble_shader_code(const std::vector<fs::path>& include_directories,
                                 std::string_view code,
                                 const ResourceLoadingInput& input,
                                 const fs::path& source_file);

std::pair<bool, std::string> get_code(const ResourceLoadingInput& input, const fs::path& file_path)
{
    try {
        const auto file_id = Identifier::from_runtime_string(file_path.generic_u8string());
        const ResourceHandle file_handle = input.load_dependency<TextResource>(file_id);
        const ResourceAccessGuard include_access(file_handle);
        return { true, std::string(include_access->text()) };
    }
    catch (ResourceError&) {
        return { false, "" };
    }
}

std::pair<bool, std::string> get_include(const ResourceLoadingInput& input,
                                         const std::vector<fs::path>& include_directories,
                                         std::string_view include_file)
{
    std::string result;

    for (const fs::path& include_directory : include_directories) {
        const fs::path file_path = include_directory / include_file;

        // Load include file as a dependency of this resource.
        const auto [is_found, included_code] = get_code(input, file_path);

        if (!is_found) {
            continue;
        }

        // Add origin-tracking comment (helps debugging shader)
        result += fmt::format(k_delimiter_comment, file_path.generic_u8string());

        // Recursively assemble loaded code.
        result += assemble_shader_code(include_dirs_for_file(include_directories, file_path),
                                       included_code,
                                       input,
                                       file_path);
        return { true, result };
    }

    return { false, "" };
}

// Helper for ShaderResource::load_resource. Assemble shader code by loading included code files.
std::string assemble_shader_code(const std::vector<fs::path>& include_directories,
                                 std::string_view code,
                                 const ResourceLoadingInput& input,
                                 const fs::path& source_file)
{
    // Read line-by-line, try to parse each line as an #include directive; if successful, include
    // the file, otherwise, insert the line as is.
    std::vector<std::string_view> lines = tokenize_string(code, "\n");
    std::string assembled_code;

    for (const auto line : lines) {
        const auto [line_is_include, include_path] = try_parse_line_as_include(line);

        if (!line_is_include) {
            assembled_code += line;
            assembled_code += '\n';

            continue;
        }

        const auto [include_success,
                    included_code] = get_include(input, include_directories, include_path);
        if (!include_success) {
            throw RuntimeError{ "Could not find '{}' (#include directive in '{}'.)",
                                include_path,
                                source_file.generic_u8string() };
        }
        assembled_code += included_code;
    }

    return assembled_code;
}

} // namespace

LoadResourceResult ShaderResource::load_resource_impl(ResourceLoadingInput& input)
{
    const std::string_view shader_resource_definition = input.resource_data_as_text();

    try {
        parser::ShaderParseResult parse_result = parser::parse_shader(shader_resource_definition);

        m_parameters.reserve(parse_result.parameters.size());
        m_samplers.reserve(parse_result.samplers.size());
        m_options.reserve(parse_result.options.size());

        for (parser::ParameterDeclaration& declaration : parse_result.parameters) {
            shader::Parameter& parameter = m_parameters.emplace_back();
            parameter.name = declaration.name;
            parameter.type = declaration.type;
            declaration.value.write_binary_data(parameter.value);
        }

        for (parser::SamplerDeclaration& declaration : parse_result.samplers) {
            shader::Sampler& sampler = m_samplers.emplace_back();
            sampler.name = declaration.name;
            sampler.type = declaration.type;
        }

        for (parser::OptionDeclaration& declaration : parse_result.options) {
            m_options.push_back({ declaration.name, declaration.value });
        }

        const auto filepath = fs::path(resource_id().str_view());
        const auto include_directories = include_dirs_for_file({}, filepath);

        m_vertex_code =
            assemble_shader_code(include_directories, parse_result.vertex_code, input, filepath);
        m_fragment_code =
            assemble_shader_code(include_directories, parse_result.fragment_code, input, filepath);

        // Sort parameters so that larger types come first (for the sake of alignment)
        sort(m_parameters, [](const shader::Parameter& l, const shader::Parameter& r) {
            return static_cast<int>(l.type) > static_cast<int>(r.type);
        });

        m_tags = parse_result.tags;
    }
    catch (const std::exception& e) {
        return LoadResourceResult::data_error(e.what());
    }
    catch (const RuntimeError& e) {
        return LoadResourceResult::data_error(e.what());
    }

    return LoadResourceResult::success();
}

std::string ShaderResource::debug_print() const
{
    std::ostringstream oss;

    oss << "ShaderResource '" << resource_id().c_str() << "'\n";

    oss << "Options:\n";
    for (auto&& o : options()) {
        oss << "\t" << o.name.c_str() << "\n";
    }

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
