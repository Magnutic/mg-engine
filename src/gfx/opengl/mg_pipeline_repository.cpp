//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

#include "mg/gfx/mg_pipeline_repository.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_material.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

namespace Mg::gfx::experimental {

namespace ShaderErrorFlags {
using Value = uint32_t;
enum Flags { VertexShader = 0x1, FragmentShader = 0x1 << 1, GeometryShader = 0x1 << 2 };
} // namespace ShaderErrorFlags

// Dump code to log with line numbers
inline std::string error_dump_code(std::string_view code)
{
    std::string str;
    str.reserve(1024);
    size_t line = 1;

    str += std::to_string(line) + '\t';
    for (char c : code) {
        str += c;
        if (c == '\n') {
            ++line;
            str += std::to_string(line) + '\t';
        }
    }

    return str;
}

// Write details on shader compilation error to log.
inline void log_shader_error(const ShaderCode& code, ShaderErrorFlags::Value error_flags)
{
    if ((error_flags & ShaderErrorFlags::VertexShader) != 0) {
        g_log.write_error("Error compiling vertex shader");
        g_log.write_message(error_dump_code(code.vertex.code));
    }

    if ((error_flags & ShaderErrorFlags::GeometryShader) != 0) {
        g_log.write_error("Error compiling geometry shader");
        g_log.write_message(error_dump_code(code.geometry.code));
    }

    if ((error_flags & ShaderErrorFlags::FragmentShader) != 0) {
        g_log.write_error("Error compiling fragment shader");
        g_log.write_message(error_dump_code(code.fragment.code));
    }
}

inline ShaderCode append_shader_code(const ShaderCode& first, const ShaderCode& second)
{
    ShaderCode code = first;
    code.vertex.code += second.vertex.code;
    code.geometry.code += second.geometry.code;
    code.fragment.code += second.fragment.code;

    return code;
}

inline std::string shader_input_layout_code(const Material& material)
{
    std::string snippet;
    snippet.reserve(256);

    // Include definition of each paramater
    if (!material.parameters().empty()) {
        snippet += "layout (std140) uniform MaterialParams {\n";

        for (const Material::Parameter& p : material.parameters()) {
            snippet += '\t';
            snippet += shader_parameter_type_to_string(p.type);
            snippet += " ";
            snippet += p.name.str_view();
            snippet += ";\n";
        }

        snippet += "} material_params;\n";
    }

    // Include definition of each sampler
    for (const Material::Sampler& s : material.samplers()) {
        snippet += "uniform ";
        snippet += shader_sampler_type_to_string(s.type);
        snippet += " ";
        snippet += s.name.str_view();
        snippet += ";\n";
    }

    // Include pre-processor #defines for each enabled option
    for (const Material::Option& o : material.options()) {
        snippet += fmt::format("#define {} {:d}\n", o.c_str(), material.get_option(o));
    }

    return snippet;
}

struct ShaderCompileResult {
    Opt<VertexShaderHandle>   opt_vs;
    Opt<GeometryShaderHandle> opt_gs;
    Opt<FragmentShaderHandle> opt_fs;
    ShaderErrorFlags::Value   error_flags = 0;
};

inline ShaderCompileResult compile_shader(const ShaderCode& code)
{
    ShaderCompileResult result;

    result.opt_vs = compile_vertex_shader(code.vertex.code);
    if (!result.opt_vs) { result.error_flags |= ShaderErrorFlags::VertexShader; }

    if (!code.geometry.code.empty()) {
        result.opt_gs = compile_geometry_shader(code.geometry.code);
        if (!result.opt_gs) { result.error_flags |= ShaderErrorFlags::GeometryShader; }
    }

    if (!code.fragment.code.empty()) {
        result.opt_fs = compile_fragment_shader(code.fragment.code);
        if (!result.opt_fs) { result.error_flags |= ShaderErrorFlags::FragmentShader; }
    }

    if (result.error_flags != 0) { log_shader_error(code, result.error_flags); }

    return result;
}

Pipeline& PipelineRepository::get_pipeline(const Material& material)
{
    uint32_t hash = material.shader_hash();

    auto it = find_if(m_pipelines, [hash](auto& elem) { return elem.hash == hash; });
    if (it == m_pipelines.end()) { return make_pipeline(material).pipeline; }
    return it->pipeline;
}

PipelineRepository::PipelineNode& PipelineRepository::make_pipeline(const Material& material)
{
    const std::string_view shader_name = material.shader().resource_id().str_view();

    g_log.write_message(
        fmt::format("PipelineRepository: compiling variant of shader '{}'.", shader_name));

    ShaderCode          shader_code    = assemble_shader_code(material);
    ShaderCompileResult compile_result = compile_shader(shader_code);

    auto compile_fallback_shader = [&]() -> ShaderCompileResult {
        g_log.write_error(fmt::format("Failed to compile shader '{}'.", shader_name));
        g_log.write_message("Using error-fallback shader.");

        shader_code = append_shader_code(m_config.preamble_shader_code,
                                         m_config.on_error_shader_code);

        return compile_shader(shader_code);
    };

    if (compile_result.error_flags != 0) { compile_result = compile_fallback_shader(); }

    PipelineInputLayout additional_input_layout;
    {
        uint32_t sampler_index = 0;
        for (const Material::Sampler& sampler : material.samplers()) {
            additional_input_layout.push_back(
                { sampler.name, PipelineInputType::Sampler2D, sampler_index++ });
        }
    }

    auto log_shader_link_error = [&] {
        g_log.write_error(fmt::format("Error linking shaders for program {}.", shader_name));
        g_log.write_verbose(
            fmt::format("Vertex code:\n{}", error_dump_code(shader_code.vertex.code)));
        g_log.write_verbose(
            fmt::format("Geometry code:\n{}", error_dump_code(shader_code.geometry.code)));
        g_log.write_verbose(
            fmt::format("Fragment code:\n{}", error_dump_code(shader_code.fragment.code)));
    };

    auto make_pipeline = [&]() -> Pipeline {
        Pipeline::CreationParameters create_params{ compile_result.opt_vs.value(),
                                                    compile_result.opt_gs,
                                                    compile_result.opt_fs,
                                                    additional_input_layout,
                                                    m_config.pipeline_prototype };

        Opt<Pipeline> opt_pipeline = Pipeline::make(create_params);
        if (!opt_pipeline) {
            log_shader_link_error();

            ShaderCompileResult fallback_result = compile_fallback_shader();
            create_params.vertex_shader         = fallback_result.opt_vs.value();
            create_params.geometry_shader       = fallback_result.opt_gs;
            create_params.fragment_shader       = fallback_result.opt_fs;

            opt_pipeline = Pipeline::make(create_params);
        }

        return std::move(opt_pipeline.value());
    };

    return m_pipelines.emplace_back(PipelineNode{ make_pipeline(), material.shader_hash() });
}

ShaderCode PipelineRepository::assemble_shader_code(const Material& material)
{
    ShaderCode code = m_config.preamble_shader_code;

    // Include sampler, parameter, and enabled-option definitions
    code.vertex.code += shader_input_layout_code(material);
    code.fragment.code += shader_input_layout_code(material);

    // Access shader resource
    {
        ResourceAccessGuard shader_resource_access(material.shader());
        code.vertex.code += shader_resource_access->vertex_code();
        code.fragment.code += shader_resource_access->fragment_code();
    }

    return code;
}

} // namespace Mg::gfx::experimental

