//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_billboard_renderer.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_rotation.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_debug_renderer.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_gfx_device.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_pipeline_pool.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_gl_debug.h"
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

using namespace Mg::literals;

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

namespace {

// Binding slots for UniformBufferObjects.
constexpr uint32_t k_camera_descriptor_location = 0;
constexpr uint32_t k_material_parameters_binding_location = 1;

/** Uniform block for passing camera parameters to shader. */
struct CameraBlock {
    mat4 VP;
    mat4 P;
    vec4 cam_pos_xyz_aspect_ratio_w;
};

//--------------------------------------------------------------------------------------------------
// Shader code for billboard rendering
//--------------------------------------------------------------------------------------------------

constexpr auto billboard_vertex_shader_preamble = R"(
#version 440 core

layout(location = 0) in vec4 v_colour;
layout(location = 1) in vec3 v_position;
layout(location = 2) in float v_radius;
layout(location = 3) in float v_index;

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
#version 440 core

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
#version 440 core

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

PipelinePool make_billboard_pipeline_pool()
{
    PipelinePoolConfig config{};

    config.name = "BillboardRenderer";

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(1);
    {
        PipelineInputDescriptor& camera_block_descriptor = config.shared_input_layout[0];
        camera_block_descriptor.input_name = "CameraBlock";
        camera_block_descriptor.type = PipelineInputType::UniformBuffer;
        camera_block_descriptor.location = k_camera_descriptor_location;
        camera_block_descriptor.mandatory = true;
    }

    config.preamble_shader_code = { VertexShaderCode{ billboard_vertex_shader_preamble },
                                    GeometryShaderCode{ billboard_geometry_shader },
                                    FragmentShaderCode{ billboard_fragment_shader_preamble } };

    config.on_error_shader_code = { VertexShaderCode{ billboard_vertex_shader_fallback },
                                    GeometryShaderCode{ "" },
                                    FragmentShaderCode{ billboard_fragment_shader_fallback } };

    config.material_parameters_binding_location = k_material_parameters_binding_location;

    return PipelinePool(std::move(config));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// BillboardRendererList implementation
//--------------------------------------------------------------------------------------------------

void sort_farthest_first(const ICamera& camera, std::span<Billboard> billboards) noexcept
{
    const vec3 cam_pos = camera.get_position();

    sort(billboards, [&](const Billboard& l, const Billboard& r) {
        return glm::distance2(cam_pos, l.pos) > glm::distance2(cam_pos, r.pos);
    });
}

//--------------------------------------------------------------------------------------------------
// BillboardRenderer implementation
//--------------------------------------------------------------------------------------------------

/** Internal data for BillboardRenderer. */
struct BillboardRenderer::Impl {
    UniformBuffer camera_ubo{ sizeof(CameraBlock) };

    PipelinePool pipeline_pool = make_billboard_pipeline_pool();

    // OpenGL object ids
    BufferHandle vbo;
    VertexArrayHandle vao;
};

namespace {
// Update vertex buffer to match the new set of billboards
void update_buffer(BillboardRenderer::Impl& data, std::span<const Billboard> billboards)
{
    glBindBuffer(GL_ARRAY_BUFFER, data.vbo.as_gl_id());

    const auto vbo_size = billboards.size() * sizeof(Billboard);

    // According to the following source, this should help reduce synchronisation overhead.
    // TODO: investigate further.
    // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
    glBufferData(GL_ARRAY_BUFFER, narrow<GLsizeiptr>(vbo_size), nullptr, GL_STREAM_DRAW);

    glBufferData(GL_ARRAY_BUFFER, narrow<GLsizeiptr>(vbo_size), billboards.data(), GL_STREAM_DRAW);

    MG_CHECK_GL_ERROR();
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

    // Set up billboard data VBO.
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

    set_attrib_ptr(4); // colour
    set_attrib_ptr(3); // pos
    set_attrib_ptr(1); // radius

    glBindVertexArray(0);

    m_impl->vao.set(vao_id);
    m_impl->vbo.set(vbo_id);

    MG_CHECK_GL_ERROR();
}

BillboardRenderer::~BillboardRenderer()
{
    MG_GFX_DEBUG_GROUP("destroy BillboardRenderer")

    const auto vao_id = m_impl->vao.as_gl_id();
    const auto vbo_id = m_impl->vbo.as_gl_id();
    glDeleteVertexArrays(1, &vao_id);
    glDeleteBuffers(1, &vbo_id);
}

void BillboardRenderer::render(const IRenderTarget& render_target,
                               const ICamera& camera,
                               std::span<const Billboard> billboards,
                               const Material& material)
{
    MG_GFX_DEBUG_GROUP("BillboardRenderer::render")

    if (billboards.empty()) {
        return;
    }

    update_buffer(*m_impl, billboards);

    {
        CameraBlock camera_block{};
        camera_block.VP = camera.view_proj_matrix();
        camera_block.P = camera.proj_matrix();
        camera_block.cam_pos_xyz_aspect_ratio_w = vec4(camera.get_position(),
                                                       camera.aspect_ratio());

        m_impl->camera_ubo.set_data(byte_representation(camera_block));
    }

    const std::array shared_inputs = { PipelineInputBinding(k_camera_descriptor_location,
                                                            m_impl->camera_ubo) };
    Pipeline::bind_shared_inputs(shared_inputs);

    PipelineBindingContext binding_context;

    BindMaterialPipelineSettings settings;
    settings.culling_mode = CullingMode::none;
    settings.vertex_array = m_impl->vao;
    settings.target_framebuffer = render_target.handle();
    settings.viewport_size = render_target.image_size();
    // Enable depth write only if we are not blending with the destination buffer.
    // This will let solid billboards properly occlude each other without causing glitches for e.g.
    // additive-blended particles.
    settings.depth_write_enabled = material.blend_mode == blend_mode_constants::bm_default;
    m_impl->pipeline_pool.bind_material_pipeline(material, settings, binding_context);

    glDrawArrays(GL_POINTS, 0, as<GLint>(billboards.size()));

    MG_CHECK_GL_ERROR();
}

void BillboardRenderer::drop_shaders() noexcept
{
    MG_GFX_DEBUG_GROUP("BillboardRenderer::drop_shaders")
    m_impl->pipeline_pool.drop_pipelines();

    MG_CHECK_GL_ERROR();
}

static std::pair<vec2, float> uniform_random_disc(Random& rand)
{
    vec2 result = vec2(0.0f);
    float length_square = 0.0f;
    do {
        result.x = rand.range(-1.0f, 1.0f);
        result.y = rand.range(-1.0f, 1.0f);
        length_square = dot(result, result);
    } while (length_square > 1.0f); // Probability of repeating: 1 - pi/4 or about 21%.

    return { result, length_square };
}

static vec3 uniform_random_unit_sphere(Random& rand)
{
    const auto& [vec, length_square] = uniform_random_disc(rand);
    const float scale = std::sqrt(1.0f - length_square);
    vec3 result = vec3(0.0f);
    result.x = 2.0f * vec.x * scale;
    result.y = 2.0f * vec.y * scale;
    result.z = 1.0f - 2.0f * length_square;

    return result;
}

static vec3 uniform_random_spherical_cap(Random& rand, const float h)
{
    const auto& [vec, length_square] = uniform_random_disc(rand);
    const float k = h * length_square;
    const float scale = std::sqrt(h * (2.0f - k));
    vec3 result = vec3(0.0f);
    result.x = scale * vec.x;
    result.y = scale * vec.y;
    result.z = 1.0f - k;

    return result;
}

void ParticleSystem::emit(const size_t num)
{
    const auto quaternion = Rotation::combine(Rotation().pitch(-90_degrees),
                                              Rotation::look_to(emission_direction))
                                .to_quaternion();

    for (size_t i = 0; i < num; ++i) {
        auto particle_index = size_t(-1);
        if (!m_unused_indices.empty()) {
            particle_index = m_unused_indices.back();
            m_unused_indices.pop_back();
        }
        else {
            particle_index = m_particles.size();
            m_particles.emplace_back();
            m_velocities.emplace_back();
            m_ages.emplace_back();
        }

        Billboard& particle = m_particles[particle_index];

        const auto r = m_rand.range(0.0f, 10.0f);
        const auto g = m_rand.range(0.0f, 10.0f);
        const auto b = m_rand.range(0.0f, 10.0f);
        particle.colour = vec4(r, g, b, 1.0f);
        particle.pos = position;
        particle.radius = m_rand.range(0.015f, 0.04f);

        const float h = 1.0f - std::sin((90_degrees - emission_angle_range).radians());
        m_velocities[particle_index] = quaternion * uniform_random_spherical_cap(m_rand, h) * 3.0f;
        m_ages[particle_index] = 0.0f;
    }
}

void ParticleSystem::update(const float time_step)
{
    MG_ASSERT(m_particles.size() == m_velocities.size() && m_velocities.size() == m_ages.size());

    for (size_t i = 0; i < m_particles.size(); ++i) {
        Billboard& particle = m_particles[i];
        vec3& velocity = m_velocities[i];
        particle.pos += velocity * time_step;
        velocity += gravity * time_step;
    }

    for (size_t i = 0; i < m_ages.size(); ++i) {
        m_ages[i] += time_step;

        if (m_ages[i] > particle_lifetime) {
            Billboard& particle = m_particles[i];
            particle.colour = vec4(0.0f);
            particle.pos = vec3(0.0f);
            particle.radius = 0.0f;

            m_unused_indices.push_back(i);
        }
    }
}

} // namespace Mg::gfx
