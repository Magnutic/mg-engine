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

#include "mg_shader_factory.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_material.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_stl_helpers.h"

#include "opengl/mg_opengl_shader.h"

#include <fmt/core.h>

#include <set>

namespace Mg::gfx {

enum class ShaderCompileResult {
    Success,
    VertexShaderError,
    FragmentShaderError,
    GeometryShaderError,
    LinkingError
};

struct MakeShaderReturn {
    std::optional<ShaderHandle> opt_program;
    ShaderCompileResult         return_code;
};

static MakeShaderReturn make_shader_program(const ShaderCode& code)
{
    auto error_value = [](ShaderCompileResult enum_value) {
        return MakeShaderReturn{ std::nullopt, enum_value };
    };

    auto ovs = compile_vertex_shader(code.vertex_code);
    if (!ovs.has_value()) { return error_value(ShaderCompileResult::VertexShaderError); }
    ShaderOwner vs{ ovs.value() };

    auto ofs = compile_fragment_shader(code.fragment_code);
    if (!ofs.has_value()) { return error_value(ShaderCompileResult::FragmentShaderError); }
    ShaderOwner fs{ ofs.value() };

    std::optional<GeometryShaderHandle> ogs;
    std::optional<ShaderHandle>         o_program;

    if (code.geometry_code.empty()) {
        o_program = link_shader_program(vs.shader_handle(), std::nullopt, fs.shader_handle())
                        .value();
    }
    else {
        ogs = compile_geometry_shader(code.geometry_code);
        if (!ogs.has_value()) { return error_value(ShaderCompileResult::GeometryShaderError); }
        ShaderOwner gs{ ogs.value() };

        o_program = link_shader_program(vs.shader_handle(), gs.shader_handle(), fs.shader_handle());
    }

    if (!o_program.has_value()) { return error_value(ShaderCompileResult::LinkingError); }

    return { o_program.value(), ShaderCompileResult::Success };
}

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
inline void log_shader_error(const ShaderCode& code, ShaderCompileResult return_code)
{
    std::vector<std::pair<std::string, const std::string*>> to_dump;

    switch (return_code) {
    case ShaderCompileResult::VertexShaderError:
        g_log.write_message("Error compiling vertex shader");
        to_dump = { { "Vertex shader", &code.vertex_code } };
        break;
    case ShaderCompileResult::FragmentShaderError:
        g_log.write_message("Error compiling fragment shader");
        to_dump = { { "Fragment shader", &code.fragment_code } };
        break;
    case ShaderCompileResult::GeometryShaderError:
        g_log.write_message("Error compiling geometry shader");
        to_dump = { { "Geometry shader", &code.geometry_code } };
        break;
    case ShaderCompileResult::LinkingError:
        g_log.write_message("Linking error.");
        to_dump = { { "Vertex shader", &code.vertex_code },
                    { "Geometry shader", &code.geometry_code },
                    { "Fragment shader", &code.fragment_code } };
        break;
    case ShaderCompileResult::Success:
        MG_ASSERT(false && "unreachable");
    }

    // Dump the code for the relevant shader stages.
    for (auto&& [origin, p_shader_code] : to_dump) {
        g_log.write_message(fmt::format("{}:\n{}", origin, error_dump_code(*p_shader_code)));
    }
}

ShaderHandle ShaderFactory::make_shader(const Material& material)
{
    const char* shader_name = material.shader().resource_id().c_str();
    g_log.write_message(
        fmt::format("ShaderFactory: compiling variant of shader '{}'.", shader_name));

    const ShaderCode shader_code = m_shader_provider->make_shader_code(material);
    auto [handle, return_code]   = make_shader_program(shader_code);

    if (return_code != ShaderCompileResult::Success) {
        g_log.write_error(fmt::format("Failed to compile shader '{}'.", shader_name));
        log_shader_error(shader_code, return_code);

        g_log.write_message("Using error-fallback shader.");
        const auto fallback_code   = m_shader_provider->on_error_shader_code();
        const auto fallback_result = make_shader_program(fallback_code);

        handle = std::move(fallback_result.opt_program);
        MG_ASSERT(fallback_result.return_code == ShaderCompileResult::Success);
    }

    m_shader_handles.emplace_back(material.shader_hash(), handle.value());
    m_shader_provider->setup_shader_state(handle.value(), material);

    return handle.value();
}

void ShaderFactory::drop_shaders()
{
    for (auto [hash, handle] : m_shader_handles) { destroy_shader_program(handle); }
    m_shader_handles.clear();
}

ShaderHandle ShaderFactory::get_shader(const Material& material)
{
    uint32_t hash = material.shader_hash();

    auto it = find_if(m_shader_handles, [hash](auto& elem) { return elem.first == hash; });

    if (it == m_shader_handles.end()) { return make_shader(material); }

    return it->second;
}

} // namespace Mg::gfx
