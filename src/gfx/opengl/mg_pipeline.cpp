//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_pipeline.h"

#include "../mg_shader.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_shader_related_types.h"
#include "mg_gl_debug.h"
#include "mg_opengl_shader.h"
#include "mg_glad.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_buffer_texture.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_uniform_buffer.h"

#include "fmt/core.h"

namespace Mg::gfx {

namespace {
PipelineInputType pipeline_input_type_for(shader::SamplerType sampler_type)
{
    switch (sampler_type) {
    case shader::SamplerType::Sampler2D:
        return PipelineInputType::Sampler2D;

    case shader::SamplerType::SamplerCube:
        return PipelineInputType::SamplerCube;
    }

    MG_ASSERT(false && "Unexpected sampler_type");
}
} // namespace

//--------------------------------------------------------------------------------------------------
// PipelineInputBinding
//--------------------------------------------------------------------------------------------------

PipelineInputBinding::PipelineInputBinding(uint32_t location, const BufferTexture& buffer_texture)
    : PipelineInputBinding(location,
                           buffer_texture.handle().get(),
                           PipelineInputType::BufferTexture)
{}

PipelineInputBinding::PipelineInputBinding(uint32_t location,
                                           TextureHandle texture,
                                           shader::SamplerType sampler_type)
    : PipelineInputBinding(location, texture.get(), pipeline_input_type_for(sampler_type))
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
void bind_pipeline_input_set(std::span<const PipelineInputBinding> bindings)
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
        case PipelineInputType::SamplerCube: {
            glActiveTexture(GL_TEXTURE0 + location);
            glBindTexture(GL_TEXTURE_CUBE_MAP, gl_object_id);
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

    case PipelineInputType::SamplerCube:
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
        throw RuntimeError{
            "Mg::Pipeline::Pipeline: no such active uniform '{}' (shader-program id {}).",
            name,
            shader_handle.get()
        };
    }

    MG_CHECK_GL_ERROR();
}

} // namespace

void Pipeline::bind_shared_inputs(std::span<const PipelineInputBinding> bindings)
{
    MG_GFX_DEBUG_GROUP("Pipeline::bind_shared_inputs")
    bind_pipeline_input_set(bindings);
}

void Pipeline::bind_material_inputs(std::span<const PipelineInputBinding> bindings)
{
    MG_GFX_DEBUG_GROUP("Pipeline::bind_material_inputs")
    bind_pipeline_input_set(bindings);
}

Pipeline::Pipeline(PipelineHandle internal_handle,
                   std::span<const PipelineInputDescriptor> shared_input_layout,
                   std::span<const PipelineInputDescriptor> material_input_layout)
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

constexpr GLenum gl_polygon_mode(PolygonMode mode)
{
    constexpr enum_utils::Map<PolygonMode, GLenum> values{
        { PolygonMode::point, GL_POINT },
        { PolygonMode::line, GL_LINE },
        { PolygonMode::fill, GL_FILL },
    };
    return values[mode];
}

constexpr GLenum gl_depth_mode(DepthTestCondition mode)
{
    constexpr enum_utils::Map<DepthTestCondition, GLenum> values{
        { DepthTestCondition::less, GL_LESS },
        { DepthTestCondition::equal, GL_EQUAL },
        { DepthTestCondition::less_equal, GL_LEQUAL },
        { DepthTestCondition::greater, GL_GREATER },
        { DepthTestCondition::not_equal, GL_NOTEQUAL },
        { DepthTestCondition::greater_equal, GL_GEQUAL },
    };
    return values[mode];
}

constexpr Opt<GLenum> gl_culling_mode(CullingMode mode)
{
    if (mode == CullingMode::front) {
        return GLenum{ GL_FRONT };
    }
    if (mode == CullingMode::back) {
        return GLenum{ GL_BACK };
    }
    return nullopt;
}

constexpr GLenum gl_blend_op(BlendOp op)
{
    constexpr enum_utils::Map<BlendOp, GLenum> values{
        { BlendOp::add, GL_FUNC_ADD },
        { BlendOp::subtract, GL_FUNC_SUBTRACT },
        { BlendOp::reverse_subtract, GL_FUNC_REVERSE_SUBTRACT },
        { BlendOp::min, GL_MIN },
        { BlendOp::max, GL_MAX },
    };
    return values[op];
}

constexpr GLenum gl_blend_factor(BlendFactor factor)
{
    constexpr enum_utils::Map<BlendFactor, GLenum> values{
        { BlendFactor::zero, GL_ZERO },
        { BlendFactor::one, GL_ONE },
        { BlendFactor::src_colour, GL_SRC_COLOR },
        { BlendFactor::one_minus_src_colour, GL_ONE_MINUS_SRC_COLOR },
        { BlendFactor::src_alpha, GL_SRC_ALPHA },
        { BlendFactor::one_minus_src_alpha, GL_ONE_MINUS_SRC_ALPHA },
        { BlendFactor::dst_alpha, GL_DST_ALPHA },
        { BlendFactor::one_minus_dst_alpha, GL_ONE_MINUS_DST_ALPHA },
        { BlendFactor::dst_colour, GL_DST_COLOR },
        { BlendFactor::one_minus_dst_colour, GL_ONE_MINUS_DST_COLOR },
    };
    return values[factor];
}

void apply_pipeline_settings(const Pipeline::Settings& settings,
                             const Opt<Pipeline::Settings>& prev_settings)
{
    const bool change_vertex_array = !prev_settings ||
                                     (prev_settings->vertex_array != settings.vertex_array);

    const bool change_target_framebuffer = !prev_settings || (prev_settings->target_framebuffer !=
                                                              settings.target_framebuffer);

    const bool change_viewport = !prev_settings || change_target_framebuffer ||
                                 (prev_settings->viewport_size != settings.viewport_size);

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

    if (change_vertex_array) {
        glBindVertexArray(settings.vertex_array.as_gl_id());
    }

    if (change_target_framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, settings.target_framebuffer.as_gl_id());

        const bool is_window_framebuffer = settings.target_framebuffer.get() == 0;
        if (!is_window_framebuffer) {
            const GLuint buffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &buffer);
        }
    }

    if (change_viewport) {
        glViewport(0, 0, settings.viewport_size.width, settings.viewport_size.height);
    }

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
        throw RuntimeError{
            "Attempting to create multiple simultaneous PipelineBindingContext instances."
        };
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
    MG_GFX_DEBUG_GROUP("PipelineBindingContext::bind_pipeline")

    if (pipeline.handle() != m_bound_handle) {
        opengl::use_program(pipeline.handle());
        m_bound_handle = pipeline.handle();
    }

    apply_pipeline_settings(settings, m_bound_settings);
    m_bound_settings = settings;
}

} // namespace Mg::gfx
