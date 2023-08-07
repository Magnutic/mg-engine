//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_pipeline_pool.
 * Creates, stores, and updates graphics pipelines from materials using a given template.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_pipeline.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"

#include <string>
#include <vector>

namespace Mg::gfx {
class Material;
}

namespace Mg::gfx {

struct VertexShaderCode {
    std::string code;
};

struct GeometryShaderCode {
    std::string code;
};

struct FragmentShaderCode {
    std::string code;
};

struct ShaderCode {
    VertexShaderCode vertex;
    GeometryShaderCode geometry;
    FragmentShaderCode fragment;
};

struct PipelinePoolConfig {
    // Name for the pipeline pool. Used for logging.
    std::string name;

    ShaderCode preamble_shader_code;
    ShaderCode on_error_shader_code;

    // Binding location for the material parameters.
    // This should be chosen to not overlap with any of the shared inputs used by the pipeline,
    // as defined in shared_input_layout.
    uint32_t material_parameters_binding_location = {};

    Array<PipelineInputDescriptor> shared_input_layout;
};

/** Check config for errors. Prints error and shuts down application if it fails.
 * Public for testability.
 */
void validate(const PipelinePoolConfig& config);

struct BindMaterialPipelineSettings {
    /** Vertex array -- the geometry to draw. */
    VertexArrayHandle vertex_array{};

    /** Which framebuffer to render onto. */
    FrameBufferHandle target_framebuffer{ 0 };

    /** Size of the rendering viewport. */
    ImageSize viewport_size = {};

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

class PipelinePool {
public:
    explicit PipelinePool(PipelinePoolConfig&& config);
    ~PipelinePool() = default;

    MG_MAKE_NON_COPYABLE(PipelinePool);
    MG_MAKE_NON_MOVABLE(PipelinePool);

    /** Binds the pipeline corresponding the the given material, creating it if needed. Requires
     * that you create a `PipelineBindingContext` first.
     */
    void bind_material_pipeline(const Material& material,
                                const BindMaterialPipelineSettings& settings,
                                PipelineBindingContext& binding_context);

    /** Drops all stored pipelines, releasing resources. This can be used to enable hot reloading of
     * material data. If the materials have changed since last binding, the changes (including
     * shader-code changes) will take effect after pipelines have been dropped, since the pipelines
     * will be regenerated on first use after dropping.
     */
    void drop_pipelines() noexcept;

    /** Drops stored pipeline for the given material, if it has been created. */
    void drop_pipeline(const Material& material) noexcept;

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

} // namespace Mg::gfx
