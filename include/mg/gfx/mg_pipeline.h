//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

/** @file mg_pipeline.h
 * Graphics pipeline.
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_shader.h"
#include "mg/gfx/mg_texture_handle.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_opaque_handle.h"
#include "mg/utils/mg_optional.h"

#include <cstdint>

namespace Mg::gfx {

class BufferTexture;
class IRenderTarget;
class UniformBuffer;

enum class PolygonMode { Points = 0x1b00, Lines = 0x1b01, Fill = 0x1b02 };

/** Types of comparison functions to use in depth testing. */
enum class DepthTestMode {
    Less = 0x201,
    Equal = 0x202,
    LessEqual = 0x203,
    Greater = 0x204,
    NotEqual = 0x205,
    GreaterEqual = 0x206,
};

/** Types of functions to use in culling. */
enum class CullingMode { None, Front = 0x404, Back = 0x405 };

/** Type of input to a rendering pipeline. */
enum class PipelineInputType { BufferTexture, Sampler2D, UniformBuffer };

struct PipelineInputLocation {
    Identifier input_name;
    PipelineInputType type;
    uint32_t location;
};

using PipelineInputLayout = small_vector<PipelineInputLocation, 10>;

struct PipelineSettings {
    PipelineSettings()
    {
        colour_write = true;
        depth_write = true;
        depth_test = true;
        enable_blending = false;
    }

    PolygonMode polygon_mode = PolygonMode::Fill;
    DepthTestMode depth_test_mode = DepthTestMode::LessEqual;
    CullingMode culling_mode = CullingMode::Back;
    BlendMode blend_mode = c_blend_mode_alpha;

    bool colour_write : 1;
    bool depth_write : 1;
    bool depth_test : 1;
    bool enable_blending : 1;
};

/** PipelinePrototype represents shared configuration for a set of similar pipelines. */
struct PipelinePrototype {
    PipelineSettings settings;
    PipelineInputLayout common_input_layout;
};

/** A rendering pipeline: a configuration specifying which rendering parameters and shaders to use
 * when rendering a set of objects.
 */
class Pipeline {
public:
    struct CreationParameters {
        VertexShaderHandle vertex_shader;
        Opt<GeometryShaderHandle> geometry_shader;
        Opt<FragmentShaderHandle> fragment_shader;
        const PipelineInputLayout& additional_input_layout;
        const PipelinePrototype& prototype;
    };

    static Opt<Pipeline> make(const CreationParameters& params);

    ~Pipeline();

    MG_MAKE_DEFAULT_MOVABLE(Pipeline);
    MG_MAKE_NON_COPYABLE(Pipeline);

    const PipelinePrototype& prototype() const noexcept { return *m_p_prototype; };

private:
    Pipeline(OpaqueHandle internal_handle,
             const PipelinePrototype& prototype,
             const PipelineInputLayout& additional_input_layout);

    friend class PipelinePrototypeContext;

    OpaqueHandle m_internal_handle;
    const PipelinePrototype* m_p_prototype;
};

/** A `PipelinePrototypeContext` is an object which sets up the state required to bind `Pipelines`
 * of a shared `PipelinePrototype`. Hence, to bind a `Pipeline`, you must first create a
 * `PipelinePrototypeContext` using the `PipelinePrototype` corresponding to the `Pipeline` first.
 * The individual `Pipeline`s can then be bound using `PipelinePrototypeContext::bind_pipeline()`.
 */
class PipelinePrototypeContext {
public:
    PipelinePrototypeContext(const PipelinePrototype& prototype);

    MG_MAKE_NON_COPYABLE(PipelinePrototypeContext);
    MG_MAKE_NON_MOVABLE(PipelinePrototypeContext);

    void bind_pipeline(const Pipeline& pipeline) const;

    const PipelinePrototype& bound_prototype;
};

/** A set of pipeline inputs -- associations from input location index value to graphics resources,
 * specifying which resource to use for the pipeline input at the given location.
 */
class PipelineInputBinding {
public:
    PipelineInputBinding(uint32_t location, const BufferTexture& buffer_texture);
    PipelineInputBinding(uint32_t location, TextureHandle texture);
    PipelineInputBinding(uint32_t location, const UniformBuffer& ubo);

    OpaqueHandle::Value gfx_resource_handle() const { return m_gfx_resource_handle; }
    PipelineInputType type() const { return m_type; }
    uint32_t location() const { return m_location; }

private:
    PipelineInputBinding(uint32_t location, OpaqueHandle::Value handle, PipelineInputType type)
        : m_gfx_resource_handle(handle), m_type(type), m_location(location)
    {}

    OpaqueHandle::Value m_gfx_resource_handle;
    PipelineInputType m_type;
    uint32_t m_location;
};

/** Bind the set of input resources as input to pipelines. A binding remains valid even if the
 * pipeline changes, as long as the pipelines share input layout.
 */
void bind_pipeline_input_set(span<const PipelineInputBinding> bindings);

} // namespace Mg::gfx
