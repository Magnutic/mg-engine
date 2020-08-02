//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_billboard_renderer.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_gfx_device.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_pipeline_repository.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_opengl_shader.h"
#include "mg_glad.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#    define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtx/norm.hpp>
#include <glm/mat4x4.hpp>

#include <cstddef>
#include <cstring>
#include <string>

namespace Mg::gfx {

namespace {

// Binding slots for UniformBufferObjects.
constexpr uint32_t k_camera_ubo_slot = 0;
constexpr uint32_t k_material_params_ubo_slot = 1;

/** Uniform block for passing camera parameters to shader. */
struct CameraBlock {
    glm::mat4 VP;
    glm::mat4 P;
    glm::vec4 cam_pos_xyz_aspect_ratio_w;
};

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

layout (location = 0) out vec4 frag_out;

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
    void main() { frag_out = vec4(1.0, 0.0, 1.0, 1.0); }
)";

PipelineRepository make_billboard_pipeline_factory()
{
    PipelineRepository::Config config{};

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(1);
    config.shared_input_layout[0] = { "CameraBlock",
                                      PipelineInputType::UniformBuffer,
                                      k_camera_ubo_slot };

    config.preamble_shader_code = { VertexShaderCode{ billboard_vertex_shader_preamble },
                                    GeometryShaderCode{ billboard_geometry_shader },
                                    FragmentShaderCode{ billboard_fragment_shader_preamble } };

    config.on_error_shader_code = { VertexShaderCode{ billboard_vertex_shader_fallback },
                                    GeometryShaderCode{ "" },
                                    FragmentShaderCode{ billboard_fragment_shader_fallback } };

    config.material_params_ubo_slot = k_material_params_ubo_slot;

    return PipelineRepository(std::move(config));
}

Pipeline::Settings pipeline_settings()
{
    Pipeline::Settings settings;
    return settings; // default settings
}

} // namespace

//--------------------------------------------------------------------------------------------------
// BillboardRendererList implementation
//--------------------------------------------------------------------------------------------------

void BillboardRenderList::sort_farthest_first(const ICamera& camera) noexcept
{
    const glm::vec3 cam_pos = camera.get_position();

    sort(m_billboards, [&](const Billboard& l, const Billboard& r) {
        return glm::distance2(cam_pos, l.pos) > glm::distance2(cam_pos, r.pos);
    });
}

//--------------------------------------------------------------------------------------------------
// BillboardRenderer implementation
//--------------------------------------------------------------------------------------------------

/** Internal data for BillboardRenderer. */
struct BillboardRendererData {
    UniformBuffer camera_ubo{ sizeof(CameraBlock) };

    PipelineRepository pipeline_repository = make_billboard_pipeline_factory();

    // Size of VBO
    size_t vertex_buffer_size = 0;

    // OpenGL object ids
    BufferHandle vbo;
    VertexArrayHandle vao;
};

namespace {
// Update vertex buffer to match the new set of billboards
void update_buffer(BillboardRendererData& data, span<const Billboard> billboards)
{
    glBindBuffer(GL_ARRAY_BUFFER, narrow<GLuint>(data.vbo.get()));

    // According to the following source, this should help reduce synchronisation overhead.
    // TODO: investigate further.
    // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
    glBufferData(GL_ARRAY_BUFFER,
                 narrow<GLsizeiptr>(data.vertex_buffer_size),
                 nullptr,
                 GL_STREAM_DRAW);

    data.vertex_buffer_size = billboards.size() * sizeof(Billboard);
    glBufferData(GL_ARRAY_BUFFER,
                 narrow<GLsizeiptr>(data.vertex_buffer_size),
                 billboards.data(),
                 GL_STREAM_DRAW);
}
} // namespace

BillboardRenderer::BillboardRenderer()
{
    MG_GFX_DEBUG_GROUP("init BillboardRenderer")

    // Create and configure vertex buffer.
    GLuint vao_id = 0;
    GLuint vbo_id = 0;
    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

    uint32_t index = 0;
    intptr_t offset = 0;
    const int32_t stride = sizeof(Billboard);

    // Tell OpenGL how to interpret the vertex buffer.
    const auto set_attrib_ptr = [&](GLint size) {
        GLvoid* gl_offset{};
        std::memcpy(&gl_offset, &offset, sizeof(offset));
        glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, gl_offset);
        glEnableVertexAttribArray(index);
        offset += intptr_t{ size } * 4;
        ++index;
    };

    set_attrib_ptr(3); // pos
    set_attrib_ptr(4); // colour
    set_attrib_ptr(1); // radius

    glBindVertexArray(0);

    impl().vao.set(vao_id);
    impl().vbo.set(vbo_id);
}

BillboardRenderer::~BillboardRenderer()
{
    MG_GFX_DEBUG_GROUP("destroy BillboardRenderer")

    const auto vao_id = narrow<GLuint>(impl().vao.get());
    const auto vbo_id = narrow<GLuint>(impl().vbo.get());
    glDeleteVertexArrays(1, &vao_id);
    glDeleteBuffers(1, &vbo_id);
}

void BillboardRenderer::render(const ICamera& camera,
                               const BillboardRenderList& render_list,
                               const Material& material)
{
    MG_GFX_DEBUG_GROUP("BillboardRenderer::render")

    if (render_list.view().empty()) {
        return;
    }

    const auto& billboards = render_list.view();
    update_buffer(impl(), billboards);

    {
        CameraBlock camera_block{};
        camera_block.VP = camera.view_proj_matrix();
        camera_block.P = camera.proj_matrix();
        camera_block.cam_pos_xyz_aspect_ratio_w = glm::vec4(camera.get_position(),
                                                            camera.aspect_ratio());

        impl().camera_ubo.set_data(byte_representation(camera_block));
    }

    const std::array shared_inputs = { PipelineInputBinding(k_camera_ubo_slot, impl().camera_ubo) };
    Pipeline::bind_shared_inputs(shared_inputs);

    PipelineBindingContext binding_context;
    impl().pipeline_repository.bind_material_pipeline(material,
                                                          pipeline_settings(),
                                                          binding_context);

    glBindVertexArray(narrow<GLuint>(impl().vao.get()));
    glDrawArrays(GL_POINTS, 0, narrow<GLint>(billboards.size()));
}

void BillboardRenderer::drop_shaders() noexcept
{
    MG_GFX_DEBUG_GROUP("BillboardRenderer::drop_shaders")
    impl().pipeline_repository.drop_pipelines();
}

} // namespace Mg::gfx
