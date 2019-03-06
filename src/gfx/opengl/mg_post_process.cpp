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

#include "mg/gfx/mg_post_process.h"

#include "shader_factories/mg_post_process_shader_provider.h"

#include "mg_gl_debug.h"
#include "mg_gl_gfx_device.h"
#include "mg_glad.h"

#include "mg/containers/mg_small_vector.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/mg_defs.h"
#include "mg/utils/mg_opaque_handle.h"

namespace Mg::gfx {

static const float quad_vertices[] = { -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,
                                       1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f };

struct PostProcessRendererData {
    ShaderFactory shader_factory = make_post_process_shader_factory();

    UniformBuffer material_params_ubo{ defs::k_material_parameters_buffer_size };
    UniformBuffer frame_block_ubo{ sizeof(post_renderer::FrameBlock) };

    OpaqueHandle vbo;
    OpaqueHandle vao;
};

static void init(PostProcessRendererData& data)
{
    GLuint vao_id = 0;
    GLuint vbo_id = 0;

    // Create mesh
    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    MG_CHECK_GL_ERROR();

    // N.B. cannot directly use these as arguments to OpenGL functions, as they expect pointer to
    // GLuint (uint32_t) whereas these are OpaqueHandle::Value (an opaque typedef of uint64_t).
    data.vao = vao_id;
    data.vbo = vao_id;
}

static void
setup_material(PostProcessRendererData& data, const Material& material, float z_near, float z_far)
{
    using namespace post_renderer;

    ShaderHandle shader = data.shader_factory.get_shader(material);
    glUseProgram(static_cast<GLuint>(access_shader_program(shader).gfx_api_handle()));

    auto& gfx_device = opengl::OpenGLGfxDevice::get();

    auto tex_unit = k_material_texture_start_unit;
    for (const Material::Sampler& sampler : material.samplers()) {
        gfx_device.bind_texture(TextureUnit{ tex_unit++ }, sampler.sampler);
    }

    data.material_params_ubo.set_data(material.material_params_buffer());
    gfx_device.bind_uniform_buffer(k_material_params_ubo_slot, data.material_params_ubo);

    FrameBlock frame_block{ z_near, z_far };
    data.frame_block_ubo.set_data(byte_representation(frame_block));
    gfx_device.bind_uniform_buffer(k_frame_block_ubo_slot, data.frame_block_ubo);
}

PostProcessRenderer::PostProcessRenderer() : PimplMixin()
{
    init(data());
}

PostProcessRenderer::~PostProcessRenderer()
{
    GLuint vao_id = static_cast<uint32_t>(data().vao.value);
    GLuint vbo_id = static_cast<uint32_t>(data().vbo.value);

    glDeleteVertexArrays(1, &vao_id);
    glDeleteBuffers(1, &vbo_id);
}

void PostProcessRenderer::post_process(const Material& material, TextureHandle input_colour)
{
    using namespace post_renderer;
    setup_material(data(), material, 0.0f, 0.0f);

    auto& gfx_device = opengl::OpenGLGfxDevice::get();
    gfx_device.bind_texture(k_input_colour_texture_unit, input_colour);
    glActiveTexture(GL_TEXTURE0 + k_input_depth_texture_unit.get());
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint vao_id = static_cast<uint32_t>(data().vao.value);

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    MG_CHECK_GL_ERROR();
}

void PostProcessRenderer::post_process(const Material& material,
                                       TextureHandle   input_colour,
                                       TextureHandle   input_depth,
                                       float           z_near,
                                       float           z_far)
{
    using namespace post_renderer;
    setup_material(data(), material, z_near, z_far);

    auto& gfx_device = opengl::OpenGLGfxDevice::get();
    gfx_device.bind_texture(k_input_colour_texture_unit, input_colour);
    gfx_device.bind_texture(k_input_depth_texture_unit, input_depth);

    GLuint vao_id = static_cast<uint32_t>(data().vao.value);

    glBindVertexArray(vao_id);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    MG_CHECK_GL_ERROR();
}

} // namespace Mg::gfx
