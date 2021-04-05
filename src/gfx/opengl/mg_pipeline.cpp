//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_pipeline.h"

#include "../mg_shader.h"
#include "mg/core/mg_runtime_error.h"
#include "mg_opengl_shader.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_buffer_texture.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include "fmt/core.h"

namespace Mg::gfx {

//--------------------------------------------------------------------------------------------------
// PipelineInputBinding
//--------------------------------------------------------------------------------------------------

PipelineInputBinding::PipelineInputBinding(uint32_t location, const BufferTexture& buffer_texture)
    : PipelineInputBinding(location,
                           buffer_texture.handle().get(),
                           PipelineInputType::BufferTexture)
{}

PipelineInputBinding::PipelineInputBinding(uint32_t location, TextureHandle texture)
    : PipelineInputBinding(location, texture.get(), PipelineInputType::Sampler2D)
{}

PipelineInputBinding::PipelineInputBinding(uint32_t location, const UniformBuffer& ubo)
    : PipelineInputBinding(location, ubo.handle().get(), PipelineInputType::UniformBuffer)
{}

//--------------------------------------------------------------------------------------------------
// Pipeline
//--------------------------------------------------------------------------------------------------

Opt<Pipeline> Pipeline::make(const Params& params)
{
    // Note: in OpenGL, PipelineHandle refers to shader programs.
    Opt<PipelineHandle> opt_program_handle = opengl::link_shader_program(params.vertex_shader,
                                                                         params.geometry_shader,
                                                                         params.fragment_shader);
    if (opt_program_handle) {
        return Pipeline(opt_program_handle.value(),
                        params.shared_input_layout,
                        params.material_input_layout);
    }

    return nullopt;
}

namespace {

// Shared implementation used for both pipeline-input binding functions in OpenGL.
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
}

// Configure the given shader using the input descriptors.
void apply_input_descriptor(const opengl::ShaderProgramHandle& shader_handle,
                            const PipelineInputDescriptor& input_descriptor)
{
    using namespace opengl;

    const auto& [input_name, type, location, mandatory] = input_descriptor;
    const auto name = input_name.str_view();

    bool success = false;

    switch (type) {
    case PipelineInputType::BufferTexture:
        [[fallthrough]];
    case PipelineInputType::Sampler2D: {
        const Opt<UniformLocation> uniform_index = opengl::uniform_location(shader_handle, name);
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

    if (!success && mandatory) {
        log.error("Mg::Pipeline::Pipeline: no such active uniform '{}' (shader-program id {}).",
                  name,
                  shader_handle.get());
        throw RuntimeError{};
    }
}

} // namespace

void Pipeline::bind_shared_inputs(span<const PipelineInputBinding> bindings)
{
    MG_GFX_DEBUG_GROUP("Pipeline::bind_shared_inputs")
    bind_pipeline_input_set(bindings);
}

void Pipeline::bind_material_inputs(span<const PipelineInputBinding> bindings)
{
    MG_GFX_DEBUG_GROUP("Pipeline::bind_material_inputs")
    bind_pipeline_input_set(bindings);
}

Pipeline::Pipeline(PipelineHandle internal_handle,
                   span<const PipelineInputDescriptor> shared_input_layout,
                   span<const PipelineInputDescriptor> material_input_layout)
    : m_handle(internal_handle)
{
    MG_GFX_DEBUG_GROUP("Create Pipeline")

    opengl::use_program(internal_handle);

    for (auto&& location : shared_input_layout) {
        apply_input_descriptor(internal_handle, location);
    }
    for (auto&& location : material_input_layout) {
        apply_input_descriptor(internal_handle, location);
    }
}

Pipeline::~Pipeline()
{
    MG_GFX_DEBUG_GROUP("Pipeline::~Pipeline")
    opengl::destroy_shader_program(m_handle);
}

//--------------------------------------------------------------------------------------------------
// PipelineBindingContext
//--------------------------------------------------------------------------------------------------

namespace /* OpenGL helpers. */ {

GLenum gl_polygon_mode(PolygonMode mode)
{
    static constexpr std::array<GLenum, 3> values{ GL_POINT, GL_LINE, GL_FILL };
    return values.at(static_cast<size_t>(mode));
}

GLenum gl_depth_mode(DepthTestCondition mode)
{
    static constexpr std::array<GLenum, 6> values{ GL_LESS,    GL_EQUAL,    GL_LEQUAL,
                                                   GL_GREATER, GL_NOTEQUAL, GL_GEQUAL };
    return values.at(static_cast<size_t>(mode));
}

Opt<GLenum> gl_culling_mode(CullingMode mode)
{
    if (mode == CullingMode::front) {
        return GLenum{ GL_FRONT };
    }
    if (mode == CullingMode::back) {
        return GLenum{ GL_BACK };
    }
    return nullopt;
}

GLenum gl_blend_op(BlendOp op)
{
    static constexpr std::array<GLenum, 6> values{
        GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX
    };
    return values.at(static_cast<size_t>(op));
}

GLenum gl_blend_factor(BlendFactor factor)
{
    static constexpr std::array<GLenum, 10> values{ GL_ZERO,      GL_ONE,
                                                    GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
                                                    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                                    GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
                                                    GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR };
    return values.at(static_cast<size_t>(factor));
}

void apply_pipeline_settings(const Pipeline::Settings settings,
                             const Opt<Pipeline::Settings> prev_settings)
{
    const bool change_polygon_mode = !prev_settings ||
                                     (prev_settings->polygon_mode != settings.polygon_mode);

    const bool change_depth_test_condition =
        !prev_settings || (prev_settings->depth_test_condition != settings.depth_test_condition);

    const bool change_blend_enabled = !prev_settings || (prev_settings->blending_enabled !=
                                                         settings.blending_enabled);

    const bool change_blend_mode = !prev_settings ||
                                   (prev_settings->blend_mode != settings.blend_mode);

    const bool change_culling_mode = !prev_settings ||
                                     (prev_settings->culling_mode != settings.culling_mode);

    const bool change_colour_mask = !prev_settings || (prev_settings->colour_write_enabled !=
                                                       settings.colour_write_enabled);

    const bool change_depth_mask = !prev_settings || (prev_settings->depth_write_enabled !=
                                                      settings.depth_write_enabled);

    if (change_polygon_mode) {
        glPolygonMode(GL_FRONT_AND_BACK, gl_polygon_mode(settings.polygon_mode));
    }

    if (change_depth_test_condition) {
        if (settings.depth_test_condition != DepthTestCondition::always) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(gl_depth_mode(settings.depth_test_condition));
        }
        else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if (change_culling_mode) {
        Opt<GLenum> gl_mode = gl_culling_mode(settings.culling_mode);
        if (!gl_mode) {
            glDisable(GL_CULL_FACE);
        }
        else {
            glEnable(GL_CULL_FACE);
            glCullFace(gl_mode.value());
        }
    }

    if (change_blend_enabled) {
        if (settings.blending_enabled) {
            glEnable(GL_BLEND);
        }
        else {
            glDisable(GL_BLEND);
        }
    }

    if (change_blend_mode) {
        glBlendEquationSeparate(gl_blend_op(settings.blend_mode.colour_blend_op),
                                gl_blend_op(settings.blend_mode.alpha_blend_op));
        glBlendFuncSeparate(gl_blend_factor(settings.blend_mode.src_colour_factor),
                            gl_blend_factor(settings.blend_mode.dst_colour_factor),
                            gl_blend_factor(settings.blend_mode.src_alpha_factor),
                            gl_blend_factor(settings.blend_mode.dst_alpha_factor));
    }

    if (change_depth_mask) {
        glDepthMask(GLboolean{ settings.depth_write_enabled });
    }

    if (change_colour_mask) {
        const GLboolean colour_write{ settings.colour_write_enabled };
        const GLboolean alpha_write{ settings.alpha_write_enabled };
        glColorMask(colour_write, colour_write, colour_write, alpha_write);
    }
}

PipelineBindingContext* g_current_context = nullptr;

} // namespace

PipelineBindingContext::PipelineBindingContext()
{
    if (g_current_context != nullptr) {
        log.error("Attempting to create multiple simultaneous PipelineBindingContext instances.");
        throw RuntimeError{};
    }

    g_current_context = this;
}

PipelineBindingContext::~PipelineBindingContext()
{
    g_current_context = nullptr;
}

void PipelineBindingContext::bind_pipeline(const Pipeline& pipeline,
                                           const Pipeline::Settings& settings)
{
    if (pipeline.handle() == m_bound_handle) {
        return;
    }

    MG_GFX_DEBUG_GROUP("PipelineBindingContext::bind_pipeline")

    apply_pipeline_settings(settings, m_bound_settings);
    opengl::use_program(pipeline.handle());

    m_bound_handle = pipeline.handle();
    m_bound_settings = settings;
}

} // namespace Mg::gfx
