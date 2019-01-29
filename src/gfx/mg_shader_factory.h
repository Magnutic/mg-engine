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

#include <cstdint>
#include <memory>
#include <string>

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_resource_handle_fwd.h"
#include "mg/gfx/mg_shader.h"
#include "mg/utils/mg_macros.h"

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

    virtual ShaderCode make_shader_code(const Material& material) const                     = 0;
    virtual void setup_shader_state(ShaderProgram& program, const Material& material) const = 0;
};

class ShaderFactory {
public:
    enum class ShaderHandle : uintptr_t;

    explicit ShaderFactory(std::unique_ptr<IShaderProvider> shader_provider)
        : m_shader_provider(std::move(shader_provider))
    {}

    ShaderHandle get_shader(const Material& material);

private:
    ShaderHandle make_shader(const Material& material);

private:
    std::unique_ptr<IShaderProvider> m_shader_provider;

    struct ShaderNode {
        uint32_t                       shader_hash;
        ResourceHandle<ShaderResource> resource_handle;
        ShaderProgram                  program;
    };

    small_vector<ShaderNode, 64> m_shader_nodes;
};

} // namespace Mg::gfx
