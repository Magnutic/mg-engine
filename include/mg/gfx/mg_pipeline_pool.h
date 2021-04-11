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
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

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
    ShaderCode preamble_shader_code;
    ShaderCode on_error_shader_code;

    uint32_t material_params_ubo_slot;

    Array<PipelineInputDescriptor> shared_input_layout;
};

struct PipelinePoolData;

class PipelinePool : PImplMixin<PipelinePoolData> {
public:
    explicit PipelinePool(PipelinePoolConfig&& config);
    ~PipelinePool();

    MG_MAKE_NON_COPYABLE(PipelinePool);
    MG_MAKE_NON_MOVABLE(PipelinePool);

    /** Binds the pipeline corresponding the the given material, creating it if needed. Requires
     * that you create a `PipelineBindingContext` first.
     */
    void bind_material_pipeline(const Material& material,
                                const Pipeline::Settings& settings,
                                PipelineBindingContext& binding_context);

    /** Creates a pipeline corresponding to the given parameters. This will make sure it is
     * immediately available before using it with `bind_material_pipeline`. It is not necessary to
     * prepare pipelines using this function, but doing so can reduce stuttering by making sure
     * shaders are compiled in advance.
     */
    void prepare_material_pipeline(const Material& material);

    /** Drops all stored pipelines, releasing resources. This can be used to enable hot reloading of
     * material data. If the materials have changed since last binding, the changes (including
     * shader-code changes) will take effect after pipelines have been dropped, since the pipelines
     * will be regenerated on first use after dropping.
     */
    void drop_pipelines() noexcept;

    /** Drops stored pipeline for the given material, if it has been created. */
    void drop_pipeline(const Material& material) noexcept;
};

} // namespace Mg::gfx
