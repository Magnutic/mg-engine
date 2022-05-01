//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material_pool.h"

#include "mg/gfx/mg_material.h"
#include "mg/utils/mg_stl_helpers.h"

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

template<typename MaterialsT> auto find_impl(MaterialsT& materials, Identifier id)
{
    auto it = find_if(materials, [&](const Material& m) { return m.id() == id; });
    return it == materials.end() ? nullptr : &*it;
}

Material* MaterialPool::find(Identifier id)
{
    return find_impl(impl().materials, id);
}
const Material* MaterialPool::find(Identifier id) const
{
    return find_impl(impl().materials, id);
}

template<typename MaterialsT> auto get_all_materials_impl(MaterialsT& materials)
{
    using pointer = decltype(&*materials.begin());
    auto result = Array<pointer>::make_for_overwrite(materials.size());
    size_t i = 0;
    // N.B. pointer-stable container for materials means this is safe.
    for (auto&& material : materials) {
        result[i++] = &material;
    }
    return result;
}

Array<Material*> MaterialPool::get_all_materials()
{
    return get_all_materials_impl(impl().materials);
}

Array<const Material*> MaterialPool::get_all_materials() const
{
    return get_all_materials_impl(impl().materials);
}

} // namespace Mg::gfx
