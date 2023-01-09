//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_renderer.h"

#include "mg/core/mg_log.h"
#include "mg/mg_defs.h"
#include "mg_gl_debug.h"
#include "mg_glad.h"

#include "../mg_light_buffers.h"
#include "../mg_light_grid.h"

#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_light.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_matrix_uniform_handler.h"
#include "mg/gfx/mg_pipeline_pool.h"
#include "mg/gfx/mg_render_command_list.h"
#include "mg/gfx/mg_render_target.h"

#include "shader_code/mg_mesh_renderer_shader_framework.h"

#include <cstring>

namespace Mg::gfx {

namespace Mesh {
using Index = uint32_t;
}

namespace { // internal linkage

constexpr uint32_t k_matrix_ubo_slot = 0;
constexpr uint32_t k_skinning_matrices_ubo_slot = 1;
constexpr uint32_t k_frame_ubo_slot = 2;
constexpr uint32_t k_light_ubo_slot = 3;
constexpr uint32_t k_material_params_ubo_slot = 4;

constexpr uint32_t k_sampler_tile_data_index = 8;
constexpr uint32_t k_sampler_light_index_index = 9; // Index of sampler for light indices

/** Location of '_matrix_index' vertex attribute in shader code. */
constexpr uint32_t k_matrix_index_vertex_attrib_location = 8;

/** Size of M and MVP matrix arrays uploaded to GPU. */
constexpr uint32_t k_matrix_ubo_array_size = 128;

/** Size of skinning matrix array uploaded to GPU. */
constexpr uint32_t k_skinning_matrix_ubo_array_size = 128;

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

    float z_near = 0.0f;
    float z_far = 0.0f;

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

FrameBlock make_frame_block(const ICamera& camera,
                            float current_time,
                            float camera_exposure,
                            const LightGridConfig& light_grid_config)
{
    std::array<int, 4> viewport_data{};
    glGetIntegerv(GL_VIEWPORT, &viewport_data[0]);
    const glm::uvec2 viewport_size{ as<uint32_t>(viewport_data[2]),
                                    as<uint32_t>(viewport_data[3]) };

    const float scale = float(light_grid_config.grid_depth) /
                        std::log2(float(light_grid_config.grid_far_plane));

    const auto depth_range = camera.depth_range();
    const auto z_near = depth_range.near();
    const auto z_far = depth_range.far();
    const auto c = std::log2(2.0f * z_far * z_near);

    FrameBlock frame_block;
    frame_block.camera_position_and_time = glm::vec4(camera.get_position(), current_time);
    frame_block.viewport_size = viewport_size;

    frame_block.z_near = z_near;
    frame_block.z_far = z_far;

    frame_block.cluster_grid_params.z_param = glm::vec2(z_near - z_far, z_near + z_far);
    frame_block.cluster_grid_params.scale = -scale;
    frame_block.cluster_grid_params.bias = light_grid_config.depth_bias + c * scale;

    frame_block.camera_exposure = camera_exposure;

    return frame_block;
}

enum class MeshPipelinePoolKind { Static, Animated };
PipelinePool make_mesh_pipeline_pool(const MeshPipelinePoolKind kind,
                                     const LightGridConfig& light_grid_config)
{
    internal::MeshRendererFrameworkShaderParams params = {
        .matrix_array_size = k_matrix_ubo_array_size,
        .skinning_matrix_array_size =
            kind == MeshPipelinePoolKind::Animated ? k_skinning_matrix_ubo_array_size : 0,
        .matrix_index_vertex_attrib_binding_location = k_matrix_index_vertex_attrib_location,
        .light_grid_config = light_grid_config
    };

    const std::string framework_vertex_code =
        internal::mesh_renderer_vertex_shader_framework_code(params);
    const std::string framework_fragment_code =
        internal::mesh_renderer_fragment_shader_framework_code(params);

    PipelinePoolConfig config{};
    config.preamble_shader_code = { VertexShaderCode{ framework_vertex_code },
                                    {},
                                    FragmentShaderCode{ framework_fragment_code } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ mesh_fs_fallback } };

    config.shared_input_layout = Array<PipelineInputDescriptor>::make(6);
    auto& matrix_block_descriptor = config.shared_input_layout[0];
    auto& skinning_matrix_block_descriptor = config.shared_input_layout[1];
    auto& frame_block_descriptor = config.shared_input_layout[2];
    auto& light_block_descriptor = config.shared_input_layout[3];
    auto& sampler_tile_data_descriptor = config.shared_input_layout[4];
    auto& sampler_light_index_descriptor = config.shared_input_layout[5];

    matrix_block_descriptor.input_name = "MatrixBlock";
    matrix_block_descriptor.location = k_matrix_ubo_slot;
    matrix_block_descriptor.type = PipelineInputType::UniformBuffer;
    matrix_block_descriptor.mandatory = true;

    skinning_matrix_block_descriptor.input_name = "SkinningMatrixBlock";
    skinning_matrix_block_descriptor.location = k_skinning_matrices_ubo_slot;
    skinning_matrix_block_descriptor.type = PipelineInputType::UniformBuffer;
    skinning_matrix_block_descriptor.mandatory = false;

    frame_block_descriptor.input_name = "FrameBlock";
    frame_block_descriptor.location = k_frame_ubo_slot;
    frame_block_descriptor.type = PipelineInputType::UniformBuffer;
    frame_block_descriptor.mandatory = true;

    light_block_descriptor.input_name = "LightBlock";
    light_block_descriptor.type = PipelineInputType::UniformBuffer;
    light_block_descriptor.location = k_light_ubo_slot;
    light_block_descriptor.mandatory = false;

    sampler_tile_data_descriptor.input_name = "_sampler_tile_data";
    sampler_tile_data_descriptor.type = PipelineInputType::BufferTexture;
    sampler_tile_data_descriptor.location = k_sampler_tile_data_index;
    sampler_tile_data_descriptor.mandatory = false;

    sampler_light_index_descriptor.input_name = "_sampler_light_index";
    sampler_light_index_descriptor.type = PipelineInputType::BufferTexture;
    sampler_light_index_descriptor.location = k_sampler_light_index_index;
    sampler_light_index_descriptor.mandatory = false;

    config.material_params_ubo_slot = k_material_params_ubo_slot;

    return PipelinePool(std::move(config));
}

} // namespace

/** MeshRenderer's state. */
struct MeshRenderer::Impl {
    Impl(const LightGridConfig& light_grid_config)
        : static_mesh_pipeline_pool(
              make_mesh_pipeline_pool(MeshPipelinePoolKind::Static, light_grid_config))
        , animated_mesh_pipeline_pool(
              make_mesh_pipeline_pool(MeshPipelinePoolKind::Animated, light_grid_config))
        , light_buffers(light_grid_config)
    {}

    PipelinePool static_mesh_pipeline_pool;
    PipelinePool animated_mesh_pipeline_pool;

    MatrixUniformHandler matrix_uniform_handler{ k_matrix_ubo_array_size, 2 };
    MatrixUniformHandler skinning_matrix_uniform_handler{ k_skinning_matrix_ubo_array_size, 1 };

    // Frame-global uniform buffer
    UniformBuffer frame_ubo{ sizeof(FrameBlock) };

    LightBuffers light_buffers;

    uint32_t num_lights = 0;
};

namespace {

void bind_shared_inputs(MeshRenderer::Impl& data, const ICamera& cam, RenderParameters params)
{
    // Upload frame-global uniforms
    const auto frame_block = make_frame_block(cam,
                                              params.current_time,
                                              params.camera_exposure,
                                              data.light_buffers.config());
    data.frame_ubo.set_data(byte_representation(frame_block));

    const std::array shared_bindings = {
        PipelineInputBinding{ k_matrix_ubo_slot, data.matrix_uniform_handler.ubo() },
        PipelineInputBinding{ k_skinning_matrices_ubo_slot,
                              data.skinning_matrix_uniform_handler.ubo() },
        PipelineInputBinding{ k_frame_ubo_slot, data.frame_ubo },
        PipelineInputBinding{ k_light_ubo_slot, data.light_buffers.light_block_buffer },
        PipelineInputBinding{ k_sampler_tile_data_index, data.light_buffers.clusters_texture },
        PipelineInputBinding{ k_sampler_light_index_index, data.light_buffers.light_index_texture }
    };
    Pipeline::bind_shared_inputs(shared_bindings);
}

BindMaterialPipelineSettings make_pipeline_settings(const IRenderTarget& render_target,
                                                    VertexArrayHandle vertex_array)
{
    BindMaterialPipelineSettings settings;
    settings.target_framebuffer = render_target.handle();
    settings.viewport_size = render_target.image_size();
    settings.vertex_array = vertex_array;
    return settings;
}

void draw_elements(size_t num_elements, size_t starting_element) noexcept
{
    const uintptr_t begin = starting_element * sizeof(Mesh::Index);

    static_assert(std::is_same_v<gfx::Mesh::Index, uint32_t>,
                  "Vertex index type must match enum value below.");
    constexpr GLenum gl_index_type = GL_UNSIGNED_INT;

    glDrawElements(GL_TRIANGLES,
                   as<int32_t>(num_elements),
                   gl_index_type,
                   reinterpret_cast<GLvoid*>(begin));
}

// Set the index into the matrix array for the next render command.
void set_matrix_index(uint32_t index) noexcept
{
    glVertexAttribI1ui(k_matrix_index_vertex_attrib_location, index);
}

// Upload the next batch of transformation matrices.
size_t upload_next_matrix_batch(MeshRenderer::Impl& data,
                                const RenderCommandList& command_list,
                                const size_t starting_command_index) noexcept
{
    std::array matrix_arrays{ command_list.m_transforms().subspan(starting_command_index),
                              command_list.vp_transforms().subspan(starting_command_index) };

    const size_t num_uploaded = data.matrix_uniform_handler.set_matrix_arrays(matrix_arrays);
    return num_uploaded;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// MeshRenderer implementation
//--------------------------------------------------------------------------------------------------

MeshRenderer::MeshRenderer(const LightGridConfig& light_grid_config) : m_impl(light_grid_config) {}

void MeshRenderer::render(const ICamera& cam,
                          const RenderCommandList& command_list,
                          span<const Light> lights,
                          const IRenderTarget& render_target,
                          RenderParameters params)
{
    MG_GFX_DEBUG_GROUP("Mesh_renderer::renderer")

    // Upload the data buffers used for lighting.
    m_impl->light_buffers.update(lights, cam);

    // Set up shared pipeline context and input bindings, to reduce amount of state changes during
    // the render loop.
    PipelineBindingContext binding_context;
    bind_shared_inputs(*m_impl, cam, params);

    Opt<BindMaterialPipelineSettings> previous_pipeline_settings;

    const auto render_commands = command_list.render_commands();
    const Material* previous_material = nullptr;
    bool previous_was_skinned_mesh = false;

    // Number of iterations until we have consumed the transformation matrices so far uploaded to
    // the CPU.
    size_t matrix_upload_countdown = 0;

    for (size_t i = 0; i < render_commands.size(); ++i) {
        // If we have consumed all matrices uploaded to GPU, then upload the next batch.
        if (matrix_upload_countdown == 0) {
            matrix_upload_countdown = upload_next_matrix_batch(*m_impl, command_list, i);
        }
        --matrix_upload_countdown;

        const RenderCommand& command = render_commands[i];
        const bool is_skinned_mesh = command.num_skinning_matrices > 0;
        auto* pipeline_pool = is_skinned_mesh ? &m_impl->animated_mesh_pipeline_pool
                                              : &m_impl->static_mesh_pipeline_pool;

        MG_ASSERT_DEBUG(command.material != nullptr);

        const bool should_switch_pipeline = //
            !previous_pipeline_settings.has_value() ||
            command.vertex_array != previous_pipeline_settings->vertex_array ||
            command.material != previous_material || previous_was_skinned_mesh != is_skinned_mesh;

        if (should_switch_pipeline) {
            auto pipeline_settings = make_pipeline_settings(render_target, command.vertex_array);
            pipeline_pool->bind_material_pipeline(*command.material,
                                                  pipeline_settings,
                                                  binding_context);

            previous_pipeline_settings = pipeline_settings;
            previous_material = command.material;
            previous_was_skinned_mesh = is_skinned_mesh;
        }

        // Set up mesh transform matrix index.
        set_matrix_index(i % k_matrix_ubo_array_size);

        // If render command is a skinned mesh, also upload skinning matrices.
        if (command.num_skinning_matrices > 0) {
            m_impl->skinning_matrix_uniform_handler.set_matrix_array(
                command_list.skinning_matrices().subspan(command.skinning_matrices_begin,
                                                         command.num_skinning_matrices));
        }

        // Draw submeshes
        draw_elements(command.amount, command.begin);
    }

    // Error check the traditional way once every frame to catch GL errors even in release builds
    MG_CHECK_GL_ERROR();
}

void MeshRenderer::drop_shaders()
{
    MG_GFX_DEBUG_GROUP("Mesh_renderer::drop_shaders")
    m_impl->static_mesh_pipeline_pool.drop_pipelines();
    m_impl->animated_mesh_pipeline_pool.drop_pipelines();
}

} // namespace Mg::gfx
