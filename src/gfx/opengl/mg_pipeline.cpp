//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_pipeline.h"

#include "../mg_texture_node.h"
#include "mg_gl_debug.h"
#include "mg_opengl_shader.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_buffer_texture.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include "fmt/core.h"

namespace Mg::gfx {

PipelinePrototypeContext::PipelinePrototypeContext(const PipelinePrototype& prototype)
    : bound_prototype(prototype)
{
    const auto& settings = prototype.settings;

    glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(settings.polygon_mode));

    if (settings.depth_test) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(static_cast<GLenum>(settings.depth_test_mode));
    }
    else {
        glDisable(GL_DEPTH_TEST);
    }

    if (settings.culling_mode == CullingMode::None) {
        glDisable(GL_CULL_FACE);
    }
    else {
        glEnable(GL_CULL_FACE);
        glCullFace(static_cast<GLenum>(settings.culling_mode));
    }

    if (settings.enable_blending) {
        glEnable(GL_BLEND);
        const auto colour = static_cast<GLenum>(settings.blend_mode.colour);
        const auto alpha = static_cast<GLenum>(settings.blend_mode.alpha);
        const auto src_colour = static_cast<GLenum>(settings.blend_mode.src_colour);
        const auto dst_colour = static_cast<GLenum>(settings.blend_mode.dst_colour);
        const auto src_alpha = static_cast<GLenum>(settings.blend_mode.src_alpha);
        const auto dst_alpha = static_cast<GLenum>(settings.blend_mode.dst_alpha);

        glBlendEquationSeparate(colour, alpha);
        glBlendFuncSeparate(src_colour, dst_colour, src_alpha, dst_alpha);
    }
    else {
        glDisable(GL_BLEND);
    }

    glDepthMask(GLboolean{ settings.depth_write });

    const auto colour_write = GLboolean{ settings.colour_write };
    glColorMask(colour_write, colour_write, colour_write, colour_write);

    MG_CHECK_GL_ERROR();
}

void PipelinePrototypeContext::bind_pipeline(const Pipeline& pipeline) const
{
    MG_ASSERT(&pipeline.prototype() == &bound_prototype &&
              "Pipeline bound to a PipelinePrototypeContext for a different PipelinePrototype.");

    const ShaderHandle shader_handle{ pipeline.m_internal_handle.value };
    opengl::use_program(shader_handle);

    MG_CHECK_GL_ERROR();
}

Opt<Pipeline> Pipeline::make(const CreationParameters& params)
{
    Opt<ShaderHandle> opt_program_handle = link_shader_program(params.vertex_shader,
                                                               params.geometry_shader,
                                                               params.fragment_shader);

    auto make_pipeline = [&params](ShaderHandle sh) {
        return Pipeline(OpaqueHandle{ static_cast<uint64_t>(sh) },
                        params.prototype,
                        params.additional_input_layout);
    };

    return opt_program_handle.map(make_pipeline);
}

Pipeline::Pipeline(OpaqueHandle internal_handle,
                   const PipelinePrototype& prototype,
                   const PipelineInputLayout& additional_input_layout)
    : m_internal_handle(std::move(internal_handle)), m_p_prototype(&prototype)
{
    using namespace opengl;
    const ShaderHandle shader_handle{ m_internal_handle.value };
    use_program(shader_handle);

    const auto set_input_location = [shader_handle](const PipelineInputLocation& input_location) {
        auto&& [input_name, type, location] = input_location;
        auto name = input_name.str_view();

        const auto log_error = [&] {
            g_log.write_message(fmt::format(
                "Mg::Pipeline::Pipeline: no such active uniform '{}' (shader-program id {}).",
                name,
                static_cast<uint32_t>(shader_handle)));
        };

        bool success = false;

        switch (type) {
        case PipelineInputType::BufferTexture:
            [[fallthrough]];

        case PipelineInputType::Sampler2D: {
            auto uniform_index = uniform_location(shader_handle, name);
            if (uniform_index) {
                set_sampler_binding(*uniform_index, TextureUnit{ location });
            }
            success = uniform_index.has_value();
            break;
        }

        case PipelineInputType::UniformBuffer:
            success = set_uniform_block_binding(shader_handle, name, UniformBufferSlot{ location });
            break;
        }

        if (!success) {
            log_error();
        }
    };

    for (auto&& location : prototype.common_input_layout) {
        set_input_location(location);
    }
    for (auto&& location : additional_input_layout) {
        set_input_location(location);
    }

    MG_CHECK_GL_ERROR();
}

Pipeline::~Pipeline()
{
    destroy_shader_program(ShaderHandle{ m_internal_handle.value });
}

PipelineInputBinding::PipelineInputBinding(uint32_t location, const BufferTexture& buffer_texture)
    : PipelineInputBinding(location,
                           buffer_texture.gfx_api_handle(),
                           PipelineInputType::BufferTexture)
{}

PipelineInputBinding::PipelineInputBinding(uint32_t location, TextureHandle texture)
    : PipelineInputBinding(location,
                           internal::dereference_texture_handle(texture).gfx_api_handle(),
                           PipelineInputType::Sampler2D)
{}

PipelineInputBinding::PipelineInputBinding(uint32_t location, const UniformBuffer& ubo)
    : PipelineInputBinding(location, ubo.gfx_api_handle(), PipelineInputType::UniformBuffer)
{}

void bind_pipeline_input_set(span<const PipelineInputBinding> bindings)
{
    for (const PipelineInputBinding& binding : bindings) {
        const auto gl_object_id = static_cast<GLuint>(binding.gfx_resource_handle());
        const uint32_t location = binding.location();

        switch (binding.type()) {
        case PipelineInputType::BufferTexture: {
            glActiveTexture(GL_TEXTURE0 + location);
            glBindTexture(GL_TEXTURE_BUFFER, gl_object_id);
            break;
        }
        case PipelineInputType::Sampler2D: {
            glActiveTexture(GL_TEXTURE0 + location);
            glBindTexture(GL_TEXTURE_2D, gl_object_id);
            break;
        }
        case PipelineInputType::UniformBuffer: {
            glBindBufferBase(GL_UNIFORM_BUFFER, location, gl_object_id);
            break;
        }
        }
    }

    MG_CHECK_GL_ERROR();
}

} // namespace Mg::gfx
