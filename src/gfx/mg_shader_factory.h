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

/** @file mg_shader_factory.
 * Generic factory for shader programs.
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/gfx/mg_shader.h"
#include "mg/utils/mg_macros.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Mg {
class ShaderResource;
} // namespace Mg

namespace Mg::gfx {

class Material;

struct ShaderCode {
    std::string vertex_code;
    std::string fragment_code;
    std::string geometry_code; // May be empty.
};

class IShaderProvider {
public:
    MG_INTERFACE_BOILERPLATE(IShaderProvider);

    /** Code to use as fallback when shaders fail to compile. Should be something visually
     * noticeable and garish, ideally.
     *
     * @remark
     * It would, perhaps, seem like a good idea to just crash if a shader fails to compile -- it is
     * a fairly critical error -- but the main reason for not doing so is that we might be editing a
     * shader and hot-reloading it to see the effect immediately. If the application crashed every
     * time we save the shader code with an error, iterating on shaders would become quite the
     * exercise in patience.
     */
    virtual ShaderCode on_error_shader_code() const = 0;

    /** Create shader code appropriate for the given material: using its parameters and options. */
    virtual ShaderCode make_shader_code(const Material& material) const = 0;

    /** Initialise state of the given ShaderProgram (e.g. setting up bindings for samplers). */
    virtual void setup_shader_state(ShaderHandle program, const Material& material) const = 0;
};

class ShaderFactory {
public:
    explicit ShaderFactory(std::unique_ptr<IShaderProvider> shader_provider)
        : m_shader_provider(std::move(shader_provider))
    {}

    ShaderHandle get_shader(const Material& material);

    void drop_shaders();

private:
    ShaderHandle make_shader(const Material& material);

private:
    std::unique_ptr<IShaderProvider>               m_shader_provider;
    std::vector<std::pair<uint32_t, ShaderHandle>> m_shader_handles;
};

} // namespace Mg::gfx
