//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
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

/** @file gfx/mg_material.h
 * Rendering material types.
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_pipeline_identifier.h"
#include "mg/gfx/mg_texture_handle.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/resources/mg_shader_types.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"

#include "mg/mg_defs.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>

namespace Mg {
class ShaderResource;
} // namespace Mg

namespace Mg::gfx {

/** Material for rendering meshes. */
class Material {
public:
    explicit Material(Identifier material_id, ResourceHandle<ShaderResource> shader);

    struct Sampler {
        Identifier name{ "" };
        shader::SamplerType type{};
        TextureHandle sampler{};
    };

    struct Parameter {
        Identifier name{ "" };
        shader::ParameterType type{};
    };

    using Option = Identifier;

    using Samplers = small_vector<Sampler, defs::k_max_samplers_per_material>;
    using Parameters = small_vector<Parameter, 4>;
    using Options = small_vector<Option, 10>;

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
    uint32_t option_flags() const noexcept { return m_option_flags; }

    /** Enable or disable the given option. Throws if the option does not exist for this material */
    void set_option(Identifier option, bool enabled);

    /** Returns whether the given option is enabled. Throws if the option does not exist. */
    bool get_option(Identifier option) const;

    void set_sampler(Identifier name, TextureHandle texture);

    Opt<size_t> sampler_index(Identifier name);

    void set_parameter(Identifier name, int param);
    void set_parameter(Identifier name, float param);
    void set_parameter(Identifier name, glm::vec2 param);
    void set_parameter(Identifier name, glm::vec4 param);

    Identifier id() const noexcept { return m_id; }

    void set_id(Identifier id) noexcept { m_id = id; }

    /** Identifier based on the aspects of the material that affect the corresponding rendering
     * pipeline. Used to allow multiple materials to re-use the same pipeline when applicable.
     */
    PipelineIdentifier pipeline_identifier() const noexcept;

    ResourceHandle<ShaderResource> shader() const noexcept { return m_shader; }

    std::string debug_print() const;

    /** Get material parameter values as a raw byte buffer. This is then passed into shaders as a
     * uniform buffer.
     */
    span<const std::byte> material_params_buffer() const noexcept
    {
        return span{ m_parameter_data }.as_bytes();
    }

private:
    void _set_parameter_impl(Identifier name,
                             span<const std::byte> param_value,
                             shader::ParameterType param_type);

private:
    Samplers m_samplers{};
    Parameters m_params{};
    Options m_options{};

    // State of Options represented as a bit-field
    uint32_t m_option_flags{};

    struct ParamsBuffer {
        uint8_t buffer[defs::k_material_parameters_buffer_size];

        const uint8_t* data() const noexcept { return &buffer[0]; }
        size_t size() const noexcept { return defs::k_material_parameters_buffer_size; }
    };

    ParamsBuffer m_parameter_data{};

    Identifier m_id{ "" };
    ResourceHandle<ShaderResource> m_shader;
};

} // namespace Mg::gfx
