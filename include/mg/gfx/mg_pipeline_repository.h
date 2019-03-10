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

#include <vector>

namespace Mg::gfx {
class Material;
}

namespace Mg::gfx::experimental {

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
    };

    struct PipelineNode {
        Pipeline pipeline;
        uint32_t hash;
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

    const PipelinePrototype& pipeline_prototype() const { return m_config.pipeline_prototype; }

    Pipeline& get_pipeline(const Material& material);

    void drop_pipelines() { m_pipelines.clear(); }

private:
    PipelineNode& make_pipeline(const Material& material);
    ShaderCode    assemble_shader_code(const Material& material);

private:
    Config                    m_config;
    std::vector<PipelineNode> m_pipelines;
};

} // namespace Mg::gfx::experimental
