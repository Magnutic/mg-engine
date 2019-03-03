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
#include "mg_gl_gfx_device.h"
#include "mg_render_command_data.h"
#include "shader_factories/mg_mesh_shader_provider.h"
#include "mg_glad.h"

#include "mg/gfx/mg_light_buffers.h"
#include "mg/gfx/mg_light_grid.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_matrix_ubo.h"
#include "mg/gfx/mg_render_command_list.h"
#include "mg/gfx/mg_vertex.h"

namespace Mg::gfx {

/** MeshRenderer's state. */
struct MeshRendererData {
    ShaderFactory shader_factory = make_mesh_shader_factory();

    MatrixUniformHandler m_matrix_uniform_handler;

    // Frame-global uniform buffer
    UniformBuffer m_frame_ubo{ sizeof(mesh_renderer::FrameBlock) };

    // Material parameters uniform buffer
    UniformBuffer m_material_params_ubo{ defs::k_material_parameters_buffer_size };

    LightBuffers m_light_buffers;
    LightGrid    m_light_grid;

    uint32_t m_num_lights = 0;

    uint32_t                    m_current_shader_hash = 0;
    ShaderFactory::ShaderHandle m_current_shader{};
};

/** Set current shader to the one required for the given material. */
inline void set_shader(MeshRendererData& data, const Material& material)
{
    const auto new_shader_hash = material.shader_hash();

    if (new_shader_hash == data.m_current_shader_hash) { return; }

    data.m_current_shader_hash = new_shader_hash;
    data.m_current_shader      = data.shader_factory.get_shader(material);

    glUseProgram(static_cast<GLuint>(data.m_current_shader));
}

/** Set shader input to match the given material. */
inline void set_material(MeshRendererData& data, const Material& material)
{
    using namespace opengl;
    using namespace mesh_renderer;

    set_shader(data, material);
    MG_ASSERT(data.m_current_shader != ShaderFactory::ShaderHandle{});

    auto& gfx_device = OpenGLGfxDevice::get();

    uint32_t tex_unit = 0;
    for (const Material::Sampler& sampler : material.samplers()) {
        gfx_device.bind_texture(TextureUnit(tex_unit++), sampler.sampler);
    }

    data.m_material_params_ubo.set_data(material.material_params_buffer());
    gfx_device.bind_uniform_buffer(k_material_params_ubo_slot, data.m_material_params_ubo);
}

/** Upload frame-constant buffers to GPU. */
inline void
upload_frame_constant_buffers(MeshRendererData& data, const ICamera& cam, RenderParameters params)
{
    using namespace opengl;
    using namespace mesh_renderer;

    auto& gfx_device = OpenGLGfxDevice::get();

    // Upload frame-global uniforms
    FrameBlock frame_block = mesh_renderer::make_frame_block(cam,
                                                             params.current_time,
                                                             params.camera_exposure);
    data.m_frame_ubo.set_data(byte_representation(frame_block));
    gfx_device.bind_uniform_buffer(k_frame_ubo_slot, data.m_frame_ubo);
    gfx_device.bind_uniform_buffer(k_light_ubo_slot, data.m_light_buffers.light_data_buffer);

    gfx_device.bind_buffer_texture(k_sampler_tile_data_index,
                                   data.m_light_buffers.tile_data_texture);
    gfx_device.bind_buffer_texture(k_sampler_light_index_index,
                                   data.m_light_buffers.light_index_texture);

    data.m_current_shader_hash = 0; // Reset to make sure that shader is set, in case current
    // shader has been changed in between invocations of this renderer's loop.
}

inline void draw_elements(size_t num_elements, size_t starting_element)
{
    auto begin = reinterpret_cast<GLvoid*>(starting_element * sizeof(uint_vertex_index)); // NOLINT
    glDrawElements(GL_TRIANGLES, int32_t(num_elements), GL_UNSIGNED_SHORT, begin);
}

inline void set_matrix_index(uint32_t index)
{
    glVertexAttribI1ui(mesh_renderer::k_matrix_index_vertex_attrib_location, index);
}

//--------------------------------------------------------------------------------------------------
// MeshRenderer implementation
//--------------------------------------------------------------------------------------------------

MeshRenderer::MeshRenderer() = default;

MeshRenderer::~MeshRenderer() = default;

void MeshRenderer::drop_shaders()
{
    data().shader_factory.drop_shaders();
}

void MeshRenderer::render(const ICamera&           cam,
                          const RenderCommandList& mesh_list,
                          span<const Light>        lights,
                          RenderParameters         params)
{
    using namespace internal;
    using namespace opengl;
    using namespace mesh_renderer;

    auto& gfx_device = OpenGLGfxDevice::get();

    auto            current_vao      = uint32_t(-1);
    const Material* current_material = nullptr;

    gfx_device.bind_uniform_buffer(k_matrix_ubo_slot, data().m_matrix_uniform_handler.ubo());

    update_light_data(data().m_light_buffers, lights, cam, data().m_light_grid);
    upload_frame_constant_buffers(data(), cam, params);

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
            set_material(data(), *material);
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
