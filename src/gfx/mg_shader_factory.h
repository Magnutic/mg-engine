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
#include "mg/core/mg_resource_handle.h"
#include "mg/gfx/mg_shader.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_pointer.h"

#include <cstdint>
#include <string>

namespace Mg {
struct FileChangedEvent;
class ResourceCache;
class ShaderResource;
} // namespace Mg

namespace Mg::gfx {

struct ShaderCode {
    std::string vertex_code;
    std::string fragment_code;
};

class Material;

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
    virtual void setup_shader_state(ShaderProgram& program, const Material& material) const = 0;
};

class ShaderFactory {
public:
    enum class ShaderHandle : uintptr_t;

    explicit ShaderFactory(Ptr<IShaderProvider> shader_provider)
        : m_shader_provider(std::move(shader_provider))
    {}

    ShaderHandle get_shader(const Material& material);

    void drop_shaders() { m_shader_nodes.clear(); }

private:
    ShaderHandle make_shader(const Material& material);

private:
    Ptr<IShaderProvider> m_shader_provider;

    struct ShaderNode {
        uint32_t      shader_hash;
        ShaderProgram program;
    };

    small_vector<ShaderNode, 64> m_shader_nodes;
};

} // namespace Mg::gfx
