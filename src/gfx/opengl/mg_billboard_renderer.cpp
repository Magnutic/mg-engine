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

#include "mg/gfx/mg_billboard_renderer.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_resource_access_guard.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/resources/mg_shader_resource.h"
#include "mg/utils/mg_object_id.h"
#include "mg/utils/mg_stl_helpers.h"

#include "../mg_shader_factory.h"
#include "mg_opengl_shader.h"
#include "mg_texture_node.h"

#include "mg_glad.h"

#include <glm/gtx/norm.hpp>
#include <glm/mat4x4.hpp>

#include <fmt/core.h>

#include <cstddef>
#include <string>

namespace Mg::gfx {

// Binding slots for UniformBufferObjects.
static constexpr UniformBufferSlot k_camera_ubo_slot{ 0 };
static constexpr UniformBufferSlot k_material_params_ubo_slot{ 1 };

//--------------------------------------------------------------------------------------------------
// Shader code for billboard rendering
//--------------------------------------------------------------------------------------------------

constexpr auto billboard_vertex_shader_preamble = R"(
#version 330 core

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec4 v_colour;
layout(location = 2) in float v_radius;

layout(std140) uniform CameraBlock {
    uniform mat4 VP;
    uniform mat4 P;
    uniform vec4 cam_pos_xyz_aspect_ratio_w;
};

#define cam_pos (cam_pos_xyz_aspect_ratio_w.xyz)
#define aspect_ratio (cam_pos_xyz_aspect_ratio_w.w)

out vec4 vs_out_colour;
out vec2 vs_out_size;
)";

constexpr auto billboard_geometry_shader = R"(
#version 330 core

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices = 4) out;

in vec4 vs_out_colour[];
in vec2 vs_out_size[];
out vec4 fs_in_colour;
out vec2 tex_coord;

void main() {
    if (vs_out_colour[0].a == 0) { return; }

    fs_in_colour = vs_out_colour[0];

    gl_Position = gl_in[0].gl_Position + vec4(-vs_out_size[0].x, -vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(0.0, 1.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(vs_out_size[0].x, -vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(1.0, 1.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(-vs_out_size[0].x, vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(vs_out_size[0].x, vs_out_size[0].y, 0.0, 0.0);
    tex_coord = vec2(1.0, 0.0);
    EmitVertex();

    EndPrimitive();
}
)";

constexpr auto billboard_fragment_shader_preamble = R"(
#version 330 core

layout (location = 0) out vec4 frag_colour;

in vec4 fs_in_colour;
in vec2 tex_coord;
)";

constexpr auto billboard_vertex_shader_fallback = R"(
void main() {
    vs_out_colour = v_colour;
    float radius = v_radius;
    gl_Position = VP * vec4(v_position, 1.0);
    vs_out_size = (P * vec4(radius, radius, 0.0, 1.0)).xy;
}
)";

constexpr auto billboard_fragment_shader_fallback = R"(
    void main() { frag_colour = vec4(1.0, 0.0, 1.0, 1.0); }
)";

class BillboardShaderProvider : public IShaderProvider {
public:
    ShaderCode on_error_shader_code() const override
    {
        ShaderCode code;
        code.vertex_code = std::string(billboard_vertex_shader_preamble) +
                           billboard_vertex_shader_fallback;
        code.fragment_code = std::string(billboard_fragment_shader_preamble) +
                             billboard_fragment_shader_fallback;
        code.geometry_code = billboard_geometry_shader;
        return code;
    }

    ShaderCode make_shader_code(const Material& material) const override
    {
        ShaderCode code;
        code.vertex_code = billboard_vertex_shader_preamble;
        code.vertex_code += shader_interface_code(material);
        code.geometry_code = billboard_geometry_shader;
        code.fragment_code = billboard_fragment_shader_preamble;
        code.fragment_code += shader_interface_code(material);

        // Access shader resource
        {
            ResourceAccessGuard shader_resource_access(material.shader());
            code.vertex_code += shader_resource_access->vertex_code();
            code.fragment_code += shader_resource_access->fragment_code();
        }

        return code;
    }

    void setup_shader_state(ShaderProgram& program, const Material& material) const override
    {
        using namespace opengl;

        use_program(program);
        set_uniform_block_binding(program, "CameraBlock", k_camera_ubo_slot);
        set_uniform_block_binding(program, "MaterialParams", k_material_params_ubo_slot);

        int32_t tex_unit = 0;

        for (const Material::Sampler& sampler : material.samplers()) {
            set_uniform(uniform_location(program, sampler.name.str_view()), tex_unit++);
        }
    }
};

inline ShaderFactory make_billboard_shader_factory()
{
    return ShaderFactory{ std::make_unique<BillboardShaderProvider>() };
}


//--------------------------------------------------------------------------------------------------
// BillboardRendererList implementation
//--------------------------------------------------------------------------------------------------

void BillboardRenderList::sort_farthest_first(const ICamera& camera)
{
    glm::vec3 cam_pos = camera.get_position();

    sort(m_billboards, [&](const Billboard& l, const Billboard& r) {
        return glm::distance2(cam_pos, l.pos) > glm::distance2(cam_pos, r.pos);
    });
}

//--------------------------------------------------------------------------------------------------
// BillboardRenderer implementation
//--------------------------------------------------------------------------------------------------

/** Uniform block for passing camera parameters to shader. */
struct CameraBlock {
    glm::mat4 VP;
    glm::mat4 P;
    glm::vec4 cam_pos_xyz_aspect_ratio_w;
};

/** Internal data for BillboardRenderer. */
struct BillboardRendererData {
    UniformBuffer material_params_ubo{ defs::k_material_parameters_buffer_size };
    UniformBuffer camera_ubo{ sizeof(CameraBlock) };

    ShaderFactory shader_factory = make_billboard_shader_factory();

    // Size of VBO
    size_t vertex_buffer_size = 0;

    // OpenGL object ids
    ObjectId vbo;
    ObjectId vao;
};

// Update vertex buffer to match the new set of billboards
inline void update_buffer(BillboardRendererData& data, span<const Billboard> billboards)
{
    glBindBuffer(GL_ARRAY_BUFFER, data.vbo.value);

    // According to the following source, this should help reduce synchronisation overhead.
    // TODO: investigate further.
    // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(data.vertex_buffer_size), nullptr, GL_STREAM_DRAW);

    data.vertex_buffer_size = billboards.size() * sizeof(Billboard);
    glBufferData(GL_ARRAY_BUFFER,
                 GLsizeiptr(data.vertex_buffer_size),
                 billboards.data(),
                 GL_STREAM_DRAW);
}

BillboardRenderer::BillboardRenderer()
{
    // Create and configure vertex buffer.
    glGenVertexArrays(1, &data().vao.value);
    glBindVertexArray(data().vao.value);

    glGenBuffers(1, &data().vbo.value);
    glBindBuffer(GL_ARRAY_BUFFER, data().vbo.value);

    uint32_t      index  = 0;
    intptr_t      offset = 0;
    const int32_t stride = sizeof(Billboard);

    // Tell OpenGL how to interpret the vertex buffer.
    auto set_attrib_ptr = [&](GLint size) {
        glVertexAttribPointer(index,
                              size,
                              GL_FLOAT,
                              GL_FALSE,
                              stride,
                              reinterpret_cast<GLvoid*>(offset)); // NOLINT
        glEnableVertexAttribArray(index);
        offset += size * 4;
        ++index;
    };

    set_attrib_ptr(3); // pos
    set_attrib_ptr(4); // colour
    set_attrib_ptr(1); // radius

    glBindVertexArray(0);
}

BillboardRenderer::~BillboardRenderer()
{
    if (data().vao.value != 0) { glDeleteVertexArrays(1, &data().vao.value); }
    if (data().vbo.value != 0) { glDeleteBuffers(1, &data().vbo.value); }
}

void BillboardRenderer::render(const ICamera&             camera,
                               const BillboardRenderList& render_list,
                               const Material&            material)
{
    if (render_list.view().empty()) { return; }

    const auto& billboards = render_list.view();
    update_buffer(data(), billboards);

    auto shader_handle = data().shader_factory.get_shader(material);

    {
        auto program_id = static_cast<GLuint>(shader_handle);
        glUseProgram(program_id);

        uint32_t tex_unit = 0;
        for (const Material::Sampler& sampler : material.samplers()) {
            auto& tex_node = internal::texture_node(sampler.sampler);
            tex_node.texture.bind_to(TextureUnit{ tex_unit++ });
        }
    }

    {
        CameraBlock camera_block{};
        camera_block.VP                         = camera.view_proj_matrix();
        camera_block.P                          = camera.proj_matrix();
        camera_block.cam_pos_xyz_aspect_ratio_w = glm::vec4(camera.get_position(),
                                                            camera.aspect_ratio());

        data().camera_ubo.set_data(byte_representation(camera_block));
        data().camera_ubo.bind_to(k_camera_ubo_slot);

        data().material_params_ubo.set_data(material.material_params_buffer());
        data().material_params_ubo.bind_to(k_material_params_ubo_slot);
    }

    glBindVertexArray(data().vao.value);
    glDrawArrays(GL_POINTS, 0, narrow<GLint>(billboards.size()));
}

} // namespace Mg::gfx
