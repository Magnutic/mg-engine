//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_pipeline_repository.
 * Creates, stores, and updates graphics pipelines from materials using a given template.
 */

#pragma once

#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_pipeline.h"
#include "mg/gfx/mg_pipeline_identifier.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/mg_defs.h"

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

class PipelineRepository {
public:
    struct Config {
        PipelinePrototype pipeline_prototype;
        ShaderCode preamble_shader_code;
        ShaderCode on_error_shader_code;
        uint32_t material_params_ubo_slot;
    };

    class BindingContext {
    private:
        friend class PipelineRepository;
        BindingContext(PipelinePrototype& prototype) : prototype_context(prototype) {}

        PipelinePrototypeContext prototype_context;
        const Pipeline* currently_bound_pipeline = nullptr;
    };

    MG_MAKE_NON_COPYABLE(PipelineRepository);
    MG_MAKE_NON_MOVABLE(PipelineRepository);

    explicit PipelineRepository(const Config& config) : m_config(config)
    {
        for (auto& input_location : config.pipeline_prototype.common_input_layout) {
            switch (input_location.type) {
            case PipelineInputType::BufferTexture:
                [[fallthrough]];
            case PipelineInputType::Sampler2D:
                MG_ASSERT(input_location.location >= 8 &&
                          "Texture slots [0,7] are reserved for material samplers.");
            default:
                break;
            }
        }
    }

    /** Create a BindingContext -- the shared binding state for all Pipelines of this
     * PipelineRepository.
     */
    BindingContext binding_context(span<const PipelineInputBinding> shared_inputs)
    {
        bind_pipeline_input_set(shared_inputs);
        return { m_config.pipeline_prototype };
    }

    /** Binds the pipeline corresponding the the given material. Requires that you create a
     * BindingContext first, see `binding_context()`.
     */
    void bind_pipeline(const Material& material, BindingContext& binding_context);

    void drop_pipelines() noexcept { m_pipelines.clear(); }

private:
    struct PipelineNode {
        Pipeline pipeline;
        PipelineIdentifier id;
    };

    Pipeline& get_or_make_pipeline(const Material& material);
    PipelineNode& make_pipeline(const Material& material);
    ShaderCode assemble_shader_code(const Material& material);

private:
    Config m_config;
    std::vector<PipelineNode> m_pipelines;
    UniformBuffer m_material_params_ubo{ defs::k_material_parameters_buffer_size };
};

} // namespace Mg::gfx
