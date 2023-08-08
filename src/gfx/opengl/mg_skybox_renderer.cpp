//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2023, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_skybox_renderer.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/gfx/mg_pipeline_pool.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Mg::gfx {

namespace {

// Binding slots for UniformBufferObjects.
constexpr uint32_t k_camera_descriptor_location = 0;
constexpr uint32_t k_material_parameters_binding_location = 1;

/** Uniform block for passing camera parameters to shader. */
struct CameraBlock {
    glm::mat4 VP;
};

//--------------------------------------------------------------------------------------------------
// Shader code for skybox rendering
//--------------------------------------------------------------------------------------------------

constexpr auto skybox_vertex_shader = R"(
#version 440 core

layout(location = 0) in vec3 v_position;

layout(std140) uniform CameraBlock {
    uniform mat4 VP;
};

out vec4 vs_out_position;

void main() {
    vs_out_position = vec4(v_position, 1.0);

    // Setting z=w will result in a depth of 1.0, putting the skybox behind all else.
    // But this has some floating-point precision issues, so we make w a little larger.
    vec4 pos = (VP * vs_out_position);
    pos.z = pos.w * 0.99999;
    gl_Position = pos;
}
)";

constexpr auto skybox_fragment_shader_preamble = R"(
#version 440 core

in vec4 vs_out_position;
layout (location = 0) out vec4 frag_out;
)";

constexpr auto skybox_fragment_shader_fallback = R"(
void main() {
    frag_out = vec4(1.0, 0.0, 1.0, 1.0);
}
)";

PipelinePool make_skybox_pipeline_pool()
{
    PipelinePoolConfig config{};
    config.name = "SkyboxRenderer";

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(1);
    {
        PipelineInputDescriptor& camera_block_descriptor = config.shared_input_layout[0];
        camera_block_descriptor.input_name = "CameraBlock";
        camera_block_descriptor.type = PipelineInputType::UniformBuffer;
        camera_block_descriptor.location = k_camera_descriptor_location;
        camera_block_descriptor.mandatory = true;
    }

    config.preamble_shader_code = { VertexShaderCode{ skybox_vertex_shader },
                                    GeometryShaderCode{ "" },
                                    FragmentShaderCode{ skybox_fragment_shader_preamble } };

    config.on_error_shader_code = { VertexShaderCode{ "" },
                                    GeometryShaderCode{ "" },
                                    FragmentShaderCode{ skybox_fragment_shader_fallback } };

    config.material_parameters_binding_location = k_material_parameters_binding_location;

    return PipelinePool(std::move(config));
}

// RAII owner for OpenGL mesh buffers
class SkyboxMesh {
public:
    VertexArrayHandle::Owner vao;
    BufferHandle::Owner vbo;
    BufferHandle::Owner ibo;

    GLsizei num_indices = 0;
};

SkyboxMesh make_skybox_mesh()
{
    constexpr std::array<glm::vec3, 8> vertices = { {
        { -1.0, -1.0, 1.0 },
        { 1.0, -1.0, 1.0 },
        { 1.0, 1.0, 1.0 },
        { -1.0, 1.0, 1.0 },
        { -1.0, -1.0, -1.0 },
        { 1.0, -1.0, -1.0 },
        { 1.0, 1.0, -1.0 },
        { -1.0, 1.0, -1.0 },
    } };

    // clang-format off
    constexpr std::array<uint32_t, 36> indices = { {
        0, 2, 1,  2, 0, 3,
        4, 1, 5,  1, 4, 0,
        5, 2, 6,  2, 5, 1,
        6, 3, 7,  3, 6, 2,
        7, 0, 4,  0, 7, 3,
        7, 5, 6,  5, 7, 4,
    } };
    // clang-format on

    // Init mesh data buffers.
    GLuint vbo_id = 0;
    glCreateBuffers(1, &vbo_id);
    glNamedBufferData(vbo_id, as<GLsizeiptr>(sizeof(vertices)), vertices.data(), GL_STATIC_DRAW);

    GLuint ibo_id = 0;
    glCreateBuffers(1, &ibo_id);
    glNamedBufferData(ibo_id, as<GLsizeiptr>(sizeof(indices)), indices.data(), GL_STATIC_DRAW);

    // Set up VAO
    GLuint vao_id = 0;
    glCreateVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    MG_CHECK_GL_ERROR();

    return { VertexArrayHandle::Owner{ vao_id },
             BufferHandle::Owner{ vbo_id },
             BufferHandle::Owner{ ibo_id },
             as<GLsizei>(indices.size()) };
}

} // namespace

struct SkyboxRenderer::Impl {
    UniformBuffer camera_ubo{ sizeof(CameraBlock) };
    PipelinePool pipeline_pool = make_skybox_pipeline_pool();
    SkyboxMesh mesh = make_skybox_mesh();
};

SkyboxRenderer::SkyboxRenderer() = default;

SkyboxRenderer::~SkyboxRenderer() = default;

void SkyboxRenderer::draw(const IRenderTarget& render_target,
                          const ICamera& camera,
                          const Material& material)
{
    MG_GFX_DEBUG_GROUP("SkyboxRenderer::draw")

    {
        CameraBlock camera_block{};
        camera_block.VP = camera.view_proj_matrix_without_translation();

        m_impl->camera_ubo.set_data(byte_representation(camera_block));
    }

    const std::array shared_inputs = { PipelineInputBinding(k_camera_descriptor_location,
                                                            m_impl->camera_ubo) };
    Pipeline::bind_shared_inputs(shared_inputs);

    PipelineBindingContext binding_context;

    BindMaterialPipelineSettings settings;
    settings.depth_write_enabled = false;
    settings.depth_test_condition = DepthTestCondition::less;
    settings.culling_mode = CullingMode::back;
    settings.vertex_array = m_impl->mesh.vao.handle;
    settings.target_framebuffer = render_target.handle();
    settings.viewport_size = render_target.image_size();
    m_impl->pipeline_pool.bind_material_pipeline(material, settings, binding_context);

    constexpr GLenum gl_index_type = GL_UNSIGNED_INT;
    glDrawElements(GL_TRIANGLES, m_impl->mesh.num_indices, gl_index_type, nullptr);
}

void SkyboxRenderer::drop_shaders()
{
    m_impl->pipeline_pool.drop_pipelines();
}

} // namespace Mg::gfx
