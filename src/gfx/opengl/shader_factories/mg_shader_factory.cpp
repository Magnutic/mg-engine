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

#include <sstream>

#include <fmt/core.h>

#include <mg/core/mg_log.h>
#include <mg/gfx/mg_material.h>
#include <mg/resources/mg_shader_resource.h>
#include <mg/utils/mg_stl_helpers.h>

#include "../mg_opengl_shader.h"

namespace Mg::gfx {

// Dump code to log with line numbers
inline std::string error_dump_code(std::string_view code)
{
    std::stringstream oss;
    size_t            line = 1;

    oss << line << '\t';
    for (char c : code) {
        oss << c;
        if (c == '\n') {
            ++line;
            oss << line << '\t';
        }
    }

    return oss.str();
}

inline ShaderProgram make_shader_program(const IShaderProvider& shader_provider,
                                         const Material&        material)
{
    ShaderCode code = shader_provider.make_shader_code(material);

    try {
        std::optional<VertexShader>   ovs = VertexShader::make(code.vertex_code);
        std::optional<FragmentShader> ofs = FragmentShader::make(code.fragment_code);

        return ShaderProgram::make(ovs.value(), ofs.value()).value();
    }
    catch (const std::bad_optional_access&) {
        const auto* shader_name = material.shader().resource_id().c_str();
        g_log.write_error(fmt::format("Failed to compile shader '{}'.", shader_name));
        g_log.write_message(fmt::format("Vertex code:\n{}", error_dump_code(code.vertex_code)));
        g_log.write_message(fmt::format("Fragment code:\n{}", error_dump_code(code.fragment_code)));

        throw;
    }
}

ShaderFactory::ShaderHandle ShaderFactory::make_shader(const Material& material)
{
    const auto* shader_name = material.shader().resource_id().c_str();
    g_log.write_message(
        fmt::format("ShaderFactory: compiling variant of shader '{}'.", shader_name));

    m_shader_nodes.push_back({ material.shader_hash(),
                               material.shader(),
                               make_shader_program(*m_shader_provider, material) });

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
