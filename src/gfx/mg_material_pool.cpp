//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material_pool.h"

#include "mg/gfx/mg_material.h"

#include <plf_colony.h>

#include <cstring> // memcpy

namespace Mg::gfx {

struct MaterialPoolData {
    plf::colony<Material> materials;
};

MaterialPool::MaterialPool() = default;
MaterialPool::~MaterialPool() = default;

Material* MaterialPool::create(Identifier id, ResourceHandle<ShaderResource> shader)
{
    const auto it = impl().materials.emplace(Material{ id, shader });
    return &(*it);
}

void MaterialPool::destroy(const Material* handle)
{
    Material* non_const_ptr = const_cast<Material*>(handle); // NOLINT
    impl().materials.erase(impl().materials.get_iterator_from_pointer(non_const_ptr));
}

} // namespace Mg::gfx
