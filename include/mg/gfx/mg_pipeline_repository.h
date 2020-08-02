//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_pipeline_repository.
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

struct PipelineRepositoryData;

class PipelineRepository : PImplMixin<PipelineRepositoryData> {
public:
    struct Config;

    explicit PipelineRepository(Config&& config);
    ~PipelineRepository();

    MG_MAKE_NON_COPYABLE(PipelineRepository);
    MG_MAKE_NON_MOVABLE(PipelineRepository);

    /** Binds the pipeline corresponding the the given material, creating it if needed. Requires
     * that you create a `PipelineBindingContext` first.
     */
    void bind_material_pipeline(const Material& material,
                                const Pipeline::Settings& settings,
                                PipelineBindingContext& binding_context);

    void drop_pipelines() noexcept;
};

struct PipelineRepository::Config {
    ShaderCode preamble_shader_code;
    ShaderCode on_error_shader_code;

    uint32_t material_params_ubo_slot;

    Array<PipelineInputDescriptor> shared_input_layout;
};

} // namespace Mg::gfx
