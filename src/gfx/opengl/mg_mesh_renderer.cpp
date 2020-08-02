//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_renderer.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include "../mg_light_buffers.h"
#include "../mg_light_grid.h"

#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_light.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_matrix_uniform_handler.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/gfx/mg_pipeline_repository.h"
#include "mg/gfx/mg_render_command_list.h"

#include "shader_code/mg_mesh_renderer_shader_framework.h"

#include <cstring>

namespace Mg::gfx {

namespace { // internal linkage

constexpr uint32_t k_matrix_ubo_slot = 0;
constexpr uint32_t k_frame_ubo_slot = 1;
constexpr uint32_t k_light_ubo_slot = 2;
constexpr uint32_t k_material_params_ubo_slot = 3;

constexpr uint32_t k_sampler_tile_data_index = 8;
constexpr uint32_t k_sampler_light_index_index = 9; // Index of sampler for light indices

/** Location of '_matrix_index' vertex attribute in shader code. */
constexpr uint32_t k_matrix_index_vertex_attrib_location = 8;

// Parameters used to calculate cluster slice from fragment depth
struct ClusterGridParams {
    glm::vec2 z_param{};
    float scale = 0.0f;
    float bias = 0.0f;
};

/** Frame-global UBO block */
struct FrameBlock {
    ClusterGridParams cluster_grid_params{};

    // .xyz: camera_position; .w: time. vec4 for alignment purposes.
    glm::vec4 camera_position_and_time{};

    glm::uvec2 viewport_size{};

    float camera_exposure = 0.0f;
};

constexpr const char* mesh_fs_fallback = R"(
float attenuate(float distance_sqr, float range_sqr_reciprocal) { return 1.0; }

vec3 light(const LightInput light, const SurfaceParams surface, const vec3 view_direction) {
    return vec3(0.0);
}

void final_colour(const SurfaceInput s_in, const SurfaceParams s, inout vec4 colour) {}

void surface(const SurfaceInput s_in, out SurfaceParams s_out) {
    s_out.albedo    = vec3(0.0);
    s_out.specular  = vec3(0.0);
    s_out.gloss     = 0.0;
    s_out.normal    = vec3(0.0);
    s_out.emission  = vec3(100.0, 0.0, 100.0);
    s_out.occlusion = 0.0;
    s_out.alpha     = 1.0;
}
)";

FrameBlock make_frame_block(const ICamera& camera, float current_time, float camera_exposure)
{
    std::array<int, 4> viewport_data{};
    glGetIntegerv(GL_VIEWPORT, &viewport_data[0]);
    const glm::uvec2 viewport_size{ narrow<uint32_t>(viewport_data[2]),
                                    narrow<uint32_t>(viewport_data[3]) };

    static const float scale = MG_LIGHT_GRID_DEPTH / std::log2(float{ MG_LIGHT_GRID_FAR_PLANE });

    const auto depth_range = camera.depth_range();
    const auto z_near = depth_range.near();
    const auto z_far = depth_range.far();
    const auto c = std::log2(2.0f * z_far * z_near);

    FrameBlock frame_block;
    frame_block.camera_position_and_time = glm::vec4(camera.get_position(), current_time);
    frame_block.viewport_size = viewport_size;

    frame_block.cluster_grid_params.z_param = glm::vec2(z_near - z_far, z_near + z_far);
    frame_block.cluster_grid_params.scale = -scale;
    frame_block.cluster_grid_params.bias = float{ MG_LIGHT_GRID_DEPTH_BIAS } + c * scale;

    frame_block.camera_exposure = camera_exposure;

    return frame_block;
}

PipelineRepository make_mesh_pipeline_repository()
{
    using namespace shader_code::mesh_renderer;

    PipelineRepository::Config config{};
    config.preamble_shader_code = { VertexShaderCode{ k_shader_framework_vertex_code },
                                    {},
                                    FragmentShaderCode{ k_shader_framework_fragment_code } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ mesh_fs_fallback } };

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(5);
    config.shared_input_layout[0] = { "MatrixBlock",
                                      PipelineInputType::UniformBuffer,
                                      k_matrix_ubo_slot },
    config.shared_input_layout[1] = { "FrameBlock",
                                      PipelineInputType::UniformBuffer,
                                      k_frame_ubo_slot },
    config.shared_input_layout[2] = { "LightBlock",
                                      PipelineInputType::UniformBuffer,
                                      k_light_ubo_slot },
    config.shared_input_layout[3] = { "_sampler_tile_data",
                                      PipelineInputType::BufferTexture,
                                      k_sampler_tile_data_index },
    config.shared_input_layout[4] = { "_sampler_light_index",
                                      PipelineInputType::BufferTexture,
                                      k_sampler_light_index_index };

    config.material_params_ubo_slot = k_material_params_ubo_slot;

    return PipelineRepository(std::move(config));
}

} // namespace

/** MeshRenderer's state. */
struct MeshRendererData {
    PipelineRepository pipeline_repository = make_mesh_pipeline_repository();

    MatrixUniformHandler m_matrix_uniform_handler;

    // Frame-global uniform buffer
    UniformBuffer m_frame_ubo{ sizeof(FrameBlock) };

    LightBuffers m_light_buffers;
    LightGrid m_light_grid;

    uint32_t m_num_lights = 0;
};

namespace {

/** Upload frame-constant buffers to GPU. */
void bind_shared_inputs(MeshRendererData& data, const ICamera& cam, RenderParameters params)
{
    // Upload frame-global uniforms
    const auto frame_block = make_frame_block(cam, params.current_time, params.camera_exposure);
    data.m_frame_ubo.set_data(byte_representation(frame_block));

    const std::array shared_bindings = {
        PipelineInputBinding{ k_matrix_ubo_slot, data.m_matrix_uniform_handler.ubo() },
        PipelineInputBinding{ k_frame_ubo_slot, data.m_frame_ubo },
        PipelineInputBinding{ k_light_ubo_slot, data.m_light_buffers.light_data_buffer },
        PipelineInputBinding{ k_sampler_tile_data_index, data.m_light_buffers.tile_data_texture },
        PipelineInputBinding{ k_sampler_light_index_index,
                              data.m_light_buffers.light_index_texture }
    };
    Pipeline::bind_shared_inputs(shared_bindings);
}

Pipeline::Settings pipeline_settings()
{
    Pipeline::Settings settings;
    return settings; // default settings
}

void draw_elements(size_t num_elements, size_t starting_element) noexcept
{
    const uintptr_t begin = starting_element * sizeof(uint_vertex_index);
    GLvoid* gl_begin{};
    std::memcpy(&gl_begin, &begin, sizeof(begin));
    glDrawElements(GL_TRIANGLES, narrow<int32_t>(num_elements), GL_UNSIGNED_SHORT, gl_begin);
}

void set_matrix_index(uint32_t index) noexcept
{
    glVertexAttribI1ui(k_matrix_index_vertex_attrib_location, index);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// MeshRenderer implementation
//--------------------------------------------------------------------------------------------------

MeshRenderer::MeshRenderer() = default;

MeshRenderer::~MeshRenderer() = default;

void MeshRenderer::drop_shaders()
{
    MG_GFX_DEBUG_GROUP("Mesh_renderer::drop_shaders")
    impl().pipeline_repository.drop_pipelines();
}

void MeshRenderer::render(const ICamera& cam,
                          const RenderCommandList& command_list,
                          span<const Light> lights,
                          RenderParameters params)
{
    MG_GFX_DEBUG_GROUP("Mesh_renderer::renderer")

    auto current_vao = uint32_t(-1);
    const Material* current_material = nullptr;

    update_light_data(impl().m_light_buffers, lights, cam, impl().m_light_grid);

    PipelineBindingContext binding_context;
    bind_shared_inputs(impl(), cam, params);

    const auto render_commands = command_list.render_commands();
    size_t matrix_update_countdown = 1;

    for (uint32_t i = 0; i < render_commands.size(); ++i) {
        if (--matrix_update_countdown == 0) {
            matrix_update_countdown = impl().m_matrix_uniform_handler.set_matrices(
                command_list.m_transform_matrices().subspan(i),
                command_list.mvp_transform_matrices().subspan(i));
        }

        const RenderCommand& command = render_commands[i];

        MG_ASSERT_DEBUG(command.material != nullptr);

        // Set up mesh state
        const auto vao_id = static_cast<GLuint>(command.vertex_array.get());

        if (current_vao != vao_id) {
            current_vao = vao_id;
            glBindVertexArray(current_vao);
        }

        // Set up material state
        if (current_material != command.material) {
            impl().pipeline_repository.bind_material_pipeline(*command.material,
                                                              pipeline_settings(),
                                                              binding_context);
            current_material = command.material;
        }

        // Set up mesh transform matrix index
        set_matrix_index(i % MATRIX_UBO_ARRAY_SIZE);

        // Draw submeshes
        draw_elements(command.amount, command.begin);
    }

    // Error check the traditional way once every frame to catch GL errors even in release builds
    MG_CHECK_GL_ERROR();
}

} // namespace Mg::gfx
