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

#include "mg/gfx/mg_mesh_renderer.h"

#include "mg_gl_debug.h"
#include "mg_render_command_data.h"
#include "shader_factories/shader_code/mg_mesh_framework_shader_code.h"
#include "mg_glad.h"

#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_light_buffers.h"
#include "mg/gfx/mg_light_grid.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_matrix_ubo.h"
#include "mg/gfx/mg_pipeline_repository.h"
#include "mg/gfx/mg_render_command_list.h"
#include "mg/gfx/mg_vertex.h"

#include "mg_texture_node.h"

namespace Mg::gfx {

namespace { // internal linkage

constexpr uint32_t k_matrix_ubo_slot          = 0;
constexpr uint32_t k_frame_ubo_slot           = 1;
constexpr uint32_t k_light_ubo_slot           = 2;
constexpr uint32_t k_material_params_ubo_slot = 3;

constexpr uint32_t k_sampler_tile_data_index   = 8;
constexpr uint32_t k_sampler_light_index_index = 9; // Index of sampler for light indices

/** Location of '_matrix_index' vertex attribute in shader code. */
constexpr uint32_t k_matrix_index_vertex_attrib_location = 8;

// Parameters used to calculate cluster slice from fragment depth
struct ClusterGridParams {
    glm::vec2 z_param;
    float     scale = 0.0f;
    float     bias  = 0.0f;
};

/** Frame-global UBO block */
struct FrameBlock {
    ClusterGridParams cluster_grid_params;

    // .xyz: camera_position; .w: time. vec4 for alignment purposes.
    glm::vec4 camera_position_and_time;

    glm::uvec2 viewport_size;

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
    glm::uvec2 viewport_size{ narrow<uint32_t>(viewport_data[2]),
                              narrow<uint32_t>(viewport_data[3]) };

    static const float scale = MG_LIGHT_GRID_DEPTH / std::log2(float(MG_LIGHT_GRID_FAR_PLANE));

    const auto depth_range = camera.depth_range();
    const auto z_near      = depth_range.near();
    const auto z_far       = depth_range.far();
    const auto c           = std::log2(2.0f * z_far * z_near);

    FrameBlock frame_block;
    frame_block.camera_position_and_time = glm::vec4(camera.get_position(), current_time);
    frame_block.viewport_size            = viewport_size;

    frame_block.cluster_grid_params.z_param = glm::vec2(z_near - z_far, z_near + z_far);
    frame_block.cluster_grid_params.scale   = -scale;
    frame_block.cluster_grid_params.bias    = float(MG_LIGHT_GRID_DEPTH_BIAS) + c * scale;

    frame_block.camera_exposure = camera_exposure;

    return frame_block;
}

experimental::PipelineRepository make_mesh_pipeline_repository()
{
    using namespace experimental;

    PipelineRepository::Config config{};
    config.preamble_shader_code = { VertexShaderCode{ k_lit_mesh_framework_vertex_code },
                                    {},
                                    FragmentShaderCode{ k_lit_mesh_framework_fragment_code } };

    config.on_error_shader_code = { {}, {}, FragmentShaderCode{ mesh_fs_fallback } };

    config.pipeline_prototype.common_input_layout = {
        { "MatrixBlock", PipelineInputType::UniformBuffer, k_matrix_ubo_slot },
        { "FrameBlock", PipelineInputType::UniformBuffer, k_frame_ubo_slot },
        { "LightBlock", PipelineInputType::UniformBuffer, k_light_ubo_slot },
        { "MaterialParams", PipelineInputType::UniformBuffer, k_material_params_ubo_slot },
        { "_sampler_tile_data", PipelineInputType::BufferTexture, k_sampler_tile_data_index },
        { "_sampler_light_index", PipelineInputType::BufferTexture, k_sampler_light_index_index }
    };

    return PipelineRepository(config);
}

} // namespace

/** MeshRenderer's state. */
struct MeshRendererData {
    experimental::PipelineRepository pipeline_repository = make_mesh_pipeline_repository();

    MatrixUniformHandler m_matrix_uniform_handler;

    // Frame-global uniform buffer
    UniformBuffer m_frame_ubo{ sizeof(FrameBlock) };

    // Material parameters uniform buffer
    UniformBuffer m_material_params_ubo{ defs::k_material_parameters_buffer_size };

    LightBuffers m_light_buffers;
    LightGrid    m_light_grid;

    uint32_t m_num_lights = 0;

    uint32_t  m_current_shader_hash = 0;
    Pipeline* m_current_pipeline{};
};

namespace {

/** Set current pipeline to the one required for the given material. */
void bind_pipeline(MeshRendererData&         data,
                   const Material&           material,
                   PipelinePrototypeContext& pipeline_prototype_context)
{
    const auto new_shader_hash = material.shader_hash();

    if (new_shader_hash == data.m_current_shader_hash) { return; }

    data.m_current_shader_hash = new_shader_hash;
    data.m_current_pipeline    = &data.pipeline_repository.get_pipeline(material);

    pipeline_prototype_context.bind_pipeline(*data.m_current_pipeline);
}

/** Set pipeline input to match the given material. */
void set_material(MeshRendererData&         data,
                  const Material&           material,
                  PipelinePrototypeContext& pipeline_prototype_context)
{
    bind_pipeline(data, material, pipeline_prototype_context);
    MG_ASSERT(data.m_current_pipeline != nullptr);

    data.m_material_params_ubo.set_data(material.material_params_buffer());

    small_vector<PipelineInputBinding, 8> input_bindings = {};

    uint32_t sampler_index = 0;
    for (const Material::Sampler& sampler : material.samplers()) {
        input_bindings.emplace_back(sampler_index++, sampler.sampler);
    }

    bind_pipeline_input_set(input_bindings);
}

/** Upload frame-constant buffers to GPU. */
void upload_frame_constant_buffers(MeshRendererData& data,
                                   const ICamera&    cam,
                                   RenderParameters  params)
{
    // Upload frame-global uniforms
    FrameBlock frame_block = make_frame_block(cam, params.current_time, params.camera_exposure);
    data.m_frame_ubo.set_data(byte_representation(frame_block));

    std::array input_bindings =
        { PipelineInputBinding{ k_matrix_ubo_slot, data.m_matrix_uniform_handler.ubo() },
          PipelineInputBinding{ k_material_params_ubo_slot, data.m_material_params_ubo },
          PipelineInputBinding{ k_frame_ubo_slot, data.m_frame_ubo },
          PipelineInputBinding{ k_light_ubo_slot, data.m_light_buffers.light_data_buffer },
          PipelineInputBinding{ k_sampler_tile_data_index, data.m_light_buffers.tile_data_texture },
          PipelineInputBinding{ k_sampler_light_index_index,
                                data.m_light_buffers.light_index_texture } };

    bind_pipeline_input_set(input_bindings);

    data.m_current_shader_hash = 0; // Reset to make sure that shader is set, in case current
    // shader has been changed in between invocations of this renderer's loop.
}

void draw_elements(size_t num_elements, size_t starting_element)
{
    auto begin = reinterpret_cast<GLvoid*>(starting_element * sizeof(uint_vertex_index)); // NOLINT
    glDrawElements(GL_TRIANGLES, int32_t(num_elements), GL_UNSIGNED_SHORT, begin);
}

void set_matrix_index(uint32_t index)
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
    data().pipeline_repository.drop_pipelines();
}

void MeshRenderer::render(const ICamera&           cam,
                          const RenderCommandList& mesh_list,
                          span<const Light>        lights,
                          RenderParameters         params)
{
    using namespace internal;

    auto            current_vao      = uint32_t(-1);
    const Material* current_material = nullptr;

    update_light_data(data().m_light_buffers, lights, cam, data().m_light_grid);
    upload_frame_constant_buffers(data(), cam, params);

    PipelinePrototypeContext pipeline_prototype_context{
        data().pipeline_repository.pipeline_prototype()
    };

    size_t next_matrix_update_index = 0;

    for (uint32_t i = 0; i < mesh_list.size(); ++i) {
        if (i == next_matrix_update_index) {
            data().m_matrix_uniform_handler.set_matrices(cam, mesh_list, i);
            next_matrix_update_index = i + MATRIX_UBO_ARRAY_SIZE;
        }

        if (mesh_list[i].culled) { continue; }
        const RenderCommandData& command_data = get_command_data(mesh_list[i].data);

        const auto* material = command_data.material;

        MG_ASSERT_DEBUG(material != nullptr);

        // Set up mesh state
        if (current_vao != command_data.mesh_vao_id) {
            current_vao = command_data.mesh_vao_id;
            glBindVertexArray(current_vao);
        }

        // Set up material state
        if (current_material != material) {
            set_material(data(), *material, pipeline_prototype_context);
            current_material = material;
        }

        // Set up mesh transform matrix index
        set_matrix_index(i % MATRIX_UBO_ARRAY_SIZE);

        // Draw submeshes
        draw_elements(command_data.amount, command_data.begin);
    }

    // Error check the traditional way once every frame to catch GL errors even in release builds
    MG_CHECK_GL_ERROR();
}

} // namespace Mg::gfx
