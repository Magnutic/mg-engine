//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material_repository.h"

#include "mg/gfx/mg_material.h"

#include <plf_colony.h>

#include <cstring> // memcpy

namespace Mg::gfx {

struct MaterialRepositoryData {
    plf::colony<Material> materials;
};

MaterialRepository::MaterialRepository() = default;
MaterialRepository::~MaterialRepository() = default;

Material* MaterialRepository::create(Identifier id, ResourceHandle<ShaderResource> shader)
{
    const auto it = impl().materials.emplace(Material{ id, shader });
    return &(*it);
}

void MaterialRepository::destroy(const Material* handle)
{
    Material* non_const_ptr = const_cast<Material*>(handle); // NOLINT
    impl().materials.erase(impl().materials.get_iterator_from_pointer(non_const_ptr));
}

} // namespace Mg::gfx
