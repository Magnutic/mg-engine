//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material_repository.h"

#include "mg/containers/mg_pooling_vector.h"
#include "mg/gfx/mg_material.h"

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
    // Size of the pools allocated for the data structure, in number of elements.
    // This is a fairly arbitrary choice: larger pools may make allocations more rare and provide
    // better data locality but could also waste space if the pool is never filled.
    static constexpr size_t k_material_node_pool_size = 512;

    PoolingVector<MaterialNode> nodes{ k_material_node_pool_size };
};

MaterialRepository::MaterialRepository() = default;
MaterialRepository::~MaterialRepository() = default;

Material* MaterialRepository::create(Identifier id, ResourceHandle<ShaderResource> shader)
{
    const auto [index, ptr] = impl().nodes.construct(Material{ id, shader });
    ptr->self_index = index;
    return &ptr->material;
}

void MaterialRepository::destroy(const Material* handle)
{
    const MaterialNode* p_node{};
    std::memcpy(&p_node, &handle, sizeof(handle));
    impl().nodes.destroy(p_node->self_index);
}

} // namespace Mg::gfx
