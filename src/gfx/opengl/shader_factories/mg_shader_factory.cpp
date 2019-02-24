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

#include "../../mg_shader_factory.h"

#include "../mg_opengl_shader.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_material.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

namespace Mg::gfx {

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

inline std::optional<ShaderProgram> compile_shader_program(ShaderCode code)
{
    std::optional<VertexShader>   ovs = VertexShader::make(code.vertex_code);
    std::optional<FragmentShader> ofs = FragmentShader::make(code.fragment_code);

    if (!ovs.has_value() || !ofs.has_value()) { return std::nullopt; }

    std::optional<GeometryShader> ogs;

    if (!code.geometry_code.empty()) {
        ogs = GeometryShader::make(code.geometry_code);

        if (!ogs.has_value()) { return std::nullopt; }

        return ShaderProgram::make(ovs.value(), ogs.value(), ofs.value());
    }

    return ShaderProgram::make(ovs.value(), ofs.value());
}

ShaderFactory::ShaderHandle ShaderFactory::make_shader(const Material& material)
{
    const char* shader_name = material.shader().resource_id().c_str();
    g_log.write_message(
        fmt::format("ShaderFactory: compiling variant of shader '{}'.", shader_name));

    const ShaderCode             code        = m_shader_provider->make_shader_code(material);
    std::optional<ShaderProgram> opt_program = compile_shader_program(code);

    if (!opt_program.has_value()) {
        g_log.write_error(fmt::format("Failed to compile shader '{}'.", shader_name));
        g_log.write_message(fmt::format("Vertex code:\n{}", error_dump_code(code.vertex_code)));

        if (!code.geometry_code.empty()) {
            g_log.write_message(
                fmt::format("Geometry code:\n{}", error_dump_code(code.vertex_code)));
        }

        g_log.write_message(fmt::format("Fragment code:\n{}", error_dump_code(code.fragment_code)));

        g_log.write_message("Using error-fallback shader.");
        opt_program = compile_shader_program(m_shader_provider->on_error_shader_code());
    }

    m_shader_nodes.push_back({ material.shader_hash(), std::move(opt_program.value()) });

    ShaderNode& node = m_shader_nodes.back();

    m_shader_provider->setup_shader_state(node.program, material);

    return ShaderHandle{ node.program.gfx_api_handle() };
}

ShaderFactory::ShaderHandle ShaderFactory::get_shader(const Material& material)
{
    uint32_t hash = material.shader_hash();

    auto node_it = find_if(m_shader_nodes, [hash](auto& node) { return node.shader_hash == hash; });

    if (node_it == m_shader_nodes.end()) { return make_shader(material); }

    return ShaderHandle{ node_it->program.gfx_api_handle() };
}

} // namespace Mg::gfx
