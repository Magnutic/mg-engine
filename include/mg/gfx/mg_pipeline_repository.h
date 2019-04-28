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

/** @file mg_pipeline_repository.
 * Creates, stores, and updates graphics pipelines from materials using a given template.
 */

#pragma once

#include "mg/gfx/mg_pipeline.h"
#include "mg/gfx/mg_shader.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/mg_defs.h"

#include <vector>

namespace Mg::gfx {
class Material;
}

namespace Mg::gfx {

template<ShaderStage stage> struct ShaderStageCode {
    ShaderStageCode() = default;
    explicit ShaderStageCode(std::string code_) : code(std::move(code_)) {}
    std::string code;
};

using VertexShaderCode   = ShaderStageCode<ShaderStage::Vertex>;
using GeometryShaderCode = ShaderStageCode<ShaderStage::Geometry>;
using FragmentShaderCode = ShaderStageCode<ShaderStage::Fragment>;

struct ShaderCode {
    VertexShaderCode   vertex;
    GeometryShaderCode geometry;
    FragmentShaderCode fragment;
};

class PipelineRepository {
public:
    struct Config {
        PipelinePrototype pipeline_prototype;
        ShaderCode        preamble_shader_code;
        ShaderCode        on_error_shader_code;
        uint32_t          material_params_ubo_slot;
    };

    struct PipelineNode {
        Pipeline pipeline;
        uint32_t hash;
    };

    class BindingContext {
    private:
        friend class PipelineRepository;
        BindingContext(PipelinePrototype& prototype) : prototype_context(prototype) {}

        PipelinePrototypeContext prototype_context;
        const Pipeline*          currently_bound_pipeline = nullptr;
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
     * PipelinePrototypeContext first, see `binding_context()`.
     */
    void bind_pipeline(const Material& material, BindingContext& binding_context);

    void drop_pipelines() { m_pipelines.clear(); }

private:
    Pipeline&     get_or_make_pipeline(const Material& material);
    PipelineNode& make_pipeline(const Material& material);
    ShaderCode    assemble_shader_code(const Material& material);

private:
    Config                    m_config;
    std::vector<PipelineNode> m_pipelines;
    UniformBuffer             m_material_params_ubo{ defs::k_material_parameters_buffer_size };
};

} // namespace Mg::gfx
