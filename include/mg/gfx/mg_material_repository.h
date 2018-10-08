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

/** @file mg_material_repository.h
 * Creates, stores, and updates materials.
 */

#pragma once

#include <memory>

#include <mg/core/mg_identifier.h>
#include <mg/core/mg_resource_handle_fwd.h>
#include <mg/utils/mg_macros.h>

namespace Mg {
class ShaderResource;
}

namespace Mg::gfx {

class Material;

class MaterialRepository {
public:
    MG_MAKE_NON_MOVABLE(MaterialRepository);
    MG_MAKE_NON_COPYABLE(MaterialRepository);

    explicit MaterialRepository();
    ~MaterialRepository();

    Material* create(Identifier id, ResourceHandle<ShaderResource> shader);

    void destroy(const Material* handle);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Mg::gfx
