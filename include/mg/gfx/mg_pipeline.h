//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_pipeline.h
 * Graphics pipeline.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_shader_related_types.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/utils/mg_enum.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"

#include <cstdint>
#include <utility>

namespace Mg::gfx {

class BufferTexture;
class IRenderTarget;
class UniformBuffer;

/** How polygons should be rasterized. */
MG_ENUM(PolygonMode, (point, line, fill))

/** Condition for letting a fragment pass through depth-testing against the depth buffer. */
MG_ENUM(DepthTestCondition, (less, equal, less_equal, greater, not_equal, greater_equal, always))

/** Which side of polygons to cull. */
MG_ENUM(CullingMode, (none, front, back))

/** The type of an input to a rendering pipeline. */
MG_ENUM(PipelineInputType, (BufferTexture, Sampler2D, SamplerCube, UniformBuffer))

/** Describes an input to a pipeline: the input's type, name, and its binding location. */
struct PipelineInputDescriptor {
    /** The name of the input as defined in shader code. */
    Identifier input_name;

    /** What type of input. */
    PipelineInputType type = {};

    /** Binding location to assign this input. */
    uint32_t location = 0;

    /** Whether the input is mandatory; is it an error if the pipeline has no such active input?
     * Note that it is not uncommon for pipelines not to have an active input even if they are
     * defined in the shader code, since they will be optimized away by the shader compiler if they
     * are unused.
     */
    bool mandatory = {};
};

/** A pipeline input binding is an association from input-location index value to a graphics
 * resource, specifying which resource to use for the pipeline input at the given location.
 */
class PipelineInputBinding {
public:
    PipelineInputBinding(uint32_t location, const BufferTexture& buffer_texture);
    PipelineInputBinding(uint32_t location, TextureHandle texture, shader::SamplerType sampler_type);
    PipelineInputBinding(uint32_t location, const UniformBuffer& ubo);

    GfxObjectHandleValue gfx_resource_handle() const { return m_gfx_resource_handle; }
    PipelineInputType type() const { return m_type; }
    uint32_t location() const { return m_location; }

private:
    PipelineInputBinding(uint32_t location, GfxObjectHandleValue handle, PipelineInputType type)
        : m_gfx_resource_handle(handle), m_type(type), m_location(location)
    {}

    GfxObjectHandleValue m_gfx_resource_handle;
    PipelineInputType m_type;
    uint32_t m_location;
};

/** A rendering pipeline: a configuration specifying which rendering parameters and shaders to use
 * when rendering a set of objects.
 */
class Pipeline {
public:
    /** Construction parameters for `Pipeline`s. */
    struct Params;

    /** Pipeline settings controlling blending, rasterization, etc. */
    struct Settings;

    /** Create a new Pipeline. May fail in case the shaders fail to link. */
    static Opt<Pipeline> make(const Params& params);

    /** Bind the given pipeline input set.
     * The binding remains valid for different Pipelines that share the same
     * Pipeline::Params::shared_input_layout.
     * @param bindings Bindings which must be compatible with the
     * PipelinePrototype::shared_input_layout of the PipelinePrototype used when creating this
     * Pipeline.
     */
    static void bind_shared_inputs(std::span<const PipelineInputBinding> bindings);

    /** Bind the given pipeline input set.
     * The binding is invalidated when another Pipeline is bound.
     * @param bindings Bindings which must be compatible with the
     * Pipeline::Params::material_input_layout used when creating this Pipeline
     */
    static void bind_material_inputs(std::span<const PipelineInputBinding> bindings);

    MG_MAKE_DEFAULT_MOVABLE(Pipeline);
    MG_MAKE_NON_COPYABLE(Pipeline);

    ~Pipeline();

    PipelineHandle handle() const { return m_handle; }

private:
    Pipeline(PipelineHandle internal_handle,
             std::span<const PipelineInputDescriptor> shared_input_layout,
             std::span<const PipelineInputDescriptor> material_input_layout);

    PipelineHandle m_handle;
};

/** Construction parameters for `Pipeline`s. */
struct Pipeline::Params {
    /** Compiled vertex shader. Mandatory. */
    VertexShaderHandle vertex_shader;

    /** Compiled fragment shader. Mandatory. */
    FragmentShaderHandle fragment_shader;

    /** Compiled geometry shader. Optional. */
    Opt<GeometryShaderHandle> geometry_shader;

    /** Input layout for shared input bindings. */
    std::span<const PipelineInputDescriptor> shared_input_layout;

    /** Input layout for material parameters and samplers. */
    std::span<const PipelineInputDescriptor> material_input_layout;
};

/** Pipeline settings controlling blending, rasterization, etc. */
struct Pipeline::Settings {
    /** Vertex array -- the geometry to draw. */
    VertexArrayHandle vertex_array{};

    /** Which framebuffer to render onto. */
    FrameBufferHandle target_framebuffer{ 0 };

    /** Size of the rendering viewport. */
    ImageSize viewport_size = {};

    /** Whether, and if so how, the colour resulting from this pipeline should be blended with
     *  previous result in render target.
     */
    BlendMode blend_mode = blend_mode_constants::bm_default;

    bool blending_enabled : 1 = false;

    /** Whether -- and if so, by which condition -- to discard fragments based on depth-test
     * against existing fragments in render target's depth buffer.
     */
    DepthTestCondition depth_test_condition : 3 = DepthTestCondition::less;

    /** How polygons should be rasterized by this pipeline. */
    PolygonMode polygon_mode : 2 = PolygonMode::fill;

    /** Which if any faces of polygons should be culled away. */
    CullingMode culling_mode : 2 = CullingMode::back;

    /** Whether to enable writing colour result of pipeline to render target. */
    bool colour_write_enabled : 1 = true;

    /** Whether to enable writing alpha-channel result of pipeline to render target. */
    bool alpha_write_enabled : 1 = true;

    /** Whether to enable writing depth result of pipeline to render target's depth buffer. */
    bool depth_write_enabled : 1 = true;
};

/** A `PipelineBindingContext` is an object which sets up the state required to bind `Pipelines`.
 *  Hence, to bind a `Pipeline`, you must first create a `PipelineBindingContext`. The individual
 * `Pipeline`s can then be bound using `PipelineBindingContext::bind_pipeline()`.
 */
class PipelineBindingContext {
public:
    explicit PipelineBindingContext();
    ~PipelineBindingContext();

    MG_MAKE_NON_COPYABLE(PipelineBindingContext);
    MG_MAKE_DEFAULT_MOVABLE(PipelineBindingContext);

    void bind_pipeline(const Pipeline& pipeline, const Pipeline::Settings& settings);

private:
    PipelineHandle m_bound_handle = PipelineHandle::null_handle();
    Opt<Pipeline::Settings> m_bound_settings;
};

} // namespace Mg::gfx
