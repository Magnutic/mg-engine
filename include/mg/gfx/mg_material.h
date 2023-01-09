//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file gfx/mg_material.h
 * Rendering material types.
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_shader_related_types.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"

#include "mg/mg_defs.h"

#include <glm/fwd.hpp>

#include <array>
#include <bitset>
#include <cstdint>
#include <string>

namespace Mg {
class Value;
class ShaderResource;
class MaterialResource;
} // namespace Mg

namespace Mg::gfx {

class Texture2D;
class TexturePool;

/** Material defining rendering parameters, such as which shader to use, which textures, and all
 * configurable inputs to the shader.
 */
class Material {
public:
    explicit Material(Identifier material_id, ResourceHandle<ShaderResource> shader_resource);

    struct Sampler {
        Identifier name;
        shader::SamplerType type{};
        Identifier texture_id;
        TextureHandle texture{};
    };

    struct Parameter {
        Identifier name;
        shader::ParameterType type{};
    };

    using Option = Identifier;

    using Samplers = small_vector<Sampler, 4>;
    using Parameters = small_vector<Parameter, 4>;
    using Options = small_vector<Option, 4>;

    using OptionFlags = std::bitset<defs::k_max_options_per_material>;

    /** `Material`s with equal `Material::PipelineId`s will have compatible `Pipeline`s. */
    struct PipelineId {
        Identifier shader_resource_id;
        Material::OptionFlags material_option_flags;
    };

    /** Whether, and if so how, the colour resulting from this pipeline should be blended with
     *  previous result in render target.
     */
    BlendMode blend_mode = blend_mode_constants::bm_default;

    /** Get the list of samplers (texture inputs) for this material. */
    const Samplers& samplers() const noexcept { return m_samplers; }

    /** Get the list of uniform input parameters for this material. */
    const Parameters& parameters() const noexcept { return m_params; }

    /** Get the list of on/off options for this material. */
    const Options& options() const noexcept { return m_options; }

    /** Get option values as bit flags. The bit at position `i` corresponds to the option at index
     * `i` within the options of this Material (see `Material::options()`).
     *
     * For a more convenient way to get the value of a given option, see `Material::get_option()`.
     *
     * @remark The engine must compile a separate version (a so-called 'permutation') of each shader
     * for each used combination of enabled options.
     * The primary use-case for this bit-flag value is to succinctly identify the corresponding
     * shader permutation for the current set of enabled options.
     */
    OptionFlags option_flags() const noexcept { return m_option_flags; }

    /** Enable or disable the given option. Throws if the option does not exist for this material */
    void set_option(Option option, bool enabled);

    /** Returns whether the given option is enabled. Throws if the option does not exist. */
    bool get_option(Option option) const;

    /** Assign a texture to a sampler. */
    void set_sampler(Identifier sampler_name, const Texture2D* texture);

    /** Get index of the sampler with the given name, if such a sampler exists. */
    Opt<size_t> sampler_index(Identifier sampler_name);

    void set_parameter(Identifier name, int param);
    void set_parameter(Identifier name, float param);
    void set_parameter(Identifier name, const glm::vec2& param);
    void set_parameter(Identifier name, const glm::vec4& param);
    void set_parameter(Identifier name, const Value& value);

    Opt<Value> get_parameter(Identifier name) const;

    /** Get identifier of this Material. */
    Identifier id() const noexcept { return m_id; }

    void set_id(Identifier id) noexcept { m_id = id; }

    /** Identifier based on the aspects of the material that affect the corresponding rendering
     * pipeline. Used to allow multiple materials to re-use the same pipeline when applicable.
     */
    Material::PipelineId pipeline_identifier() const noexcept;

    /** Get the ShaderResource on which this Material is based. */
    ResourceHandle<ShaderResource> shader() const noexcept { return m_shader_resource; }

    /** Serialize to a string, which can then be deserialized to a MaterialResource. */
    [[nodiscard]] std::string serialize() const;

    /** Get material parameter values as a raw byte buffer. This is then passed into shaders as a
     * uniform buffer.
     */
    span<const std::byte> material_params_buffer() const noexcept
    {
        return byte_representation(m_parameter_data);
    }

private:
    using ParamsBuffer = std::array<uint8_t, defs::k_material_parameters_buffer_size>;

    void _set_parameter_impl(Identifier name,
                             span<const std::byte> param_value,
                             shader::ParameterType param_type);

    Opt<std::pair<glm::vec4, shader::ParameterType>> _extract_parameter_data(Identifier name) const;

    Samplers m_samplers{};
    Parameters m_params{};
    Options m_options{};

    ParamsBuffer m_parameter_data{};

    ResourceHandle<ShaderResource> m_shader_resource;

    // State of Options represented as a bit-field
    OptionFlags m_option_flags;

    Identifier m_id;
};

//--------------------------------------------------------------------------------------------------
// Utilities for Material::PipelineId
//--------------------------------------------------------------------------------------------------

inline bool operator==(const Material::PipelineId& lhs, const Material::PipelineId& rhs)
{
    return lhs.shader_resource_id == rhs.shader_resource_id &&
           lhs.material_option_flags == rhs.material_option_flags;
}

inline bool operator!=(const Material::PipelineId& lhs, const Material::PipelineId& rhs)
{
    return !(lhs == rhs);
}

/** Comparison operator allowing the use of Material::PipelineId in a map. */
struct MaterialPipelineIdCmp {
    bool operator()(const Material::PipelineId& lhs, const Material::PipelineId& rhs) const noexcept
    {
        static_assert(sizeof(Material::OptionFlags) <= sizeof(unsigned long long));
        return Identifier::HashCompare{}(lhs.shader_resource_id, rhs.shader_resource_id) ||
               (lhs.shader_resource_id == rhs.shader_resource_id &&
                lhs.material_option_flags.to_ullong() < rhs.material_option_flags.to_ullong());
    }
};

} // namespace Mg::gfx
