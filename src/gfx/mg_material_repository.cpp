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

// The client-side handle type is Material*, but internally we use MaterialNode. Since Material is
// the first element of MaterialNode, we can reinterpret_cast Material* to MaterialNode*.

struct MaterialNode {
    explicit MaterialNode(Material&& mat) noexcept : material(std::move(mat)) {}

    // Publicly visible material data (Material* allows accessing only this).
    Material material;

    // Index of this object in data structure -- used for deletion.
    uint32_t self_index{};
};

struct MaterialRepositoryData {
    plf::colony<MaterialNode> nodes;
};

MaterialRepository::MaterialRepository() = default;
MaterialRepository::~MaterialRepository() = default;

Material* MaterialRepository::create(Identifier id, ResourceHandle<ShaderResource> shader)
{
    const auto it = impl().nodes.emplace(Material{ id, shader });
    it->self_index = narrow<uint32_t>(impl().nodes.get_index_from_iterator(it));
    return &it->material;
}

void MaterialRepository::destroy(const Material* handle)
{
    const MaterialNode* p_node{};
    std::memcpy(&p_node, &handle, sizeof(handle));
    impl().nodes.erase(impl().nodes.get_iterator_from_index(p_node->self_index));
}

} // namespace Mg::gfx
