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

#include "mg/gfx/mg_material_repository.h"

#include "mg/containers/mg_pooling_vector.h"
#include "mg/gfx/mg_material.h"

namespace Mg::gfx {

// The client-side handle type is Material*, but internally we use MaterialNode. Since Material is
// the first element of MaterialNode, we can reinterpret_cast Material* to MaterialNode*.

struct MaterialNode {
    explicit MaterialNode(Material&& mat) : material(std::move(mat)) {}

    // Publicly visible material data (Material* allows accessing only this).
    Material material;

    // Index of this object in data structure -- used for deletion.
    uint32_t self_index{};
};

class MaterialRepository::Impl {
    // Size of the pools allocated for the data structure, in number of elements.
    // This is a fairly arbitrary choice: larger pools may make allocations more rare and provide
    // better data locality but could also waste space if the pool is never filled.
    static constexpr size_t k_material_node_pool_size = 512;

public:
    PoolingVector<MaterialNode> m_nodes{ k_material_node_pool_size };
};

MaterialRepository::MaterialRepository() : m_impl(std::make_unique<Impl>()) {}
MaterialRepository::~MaterialRepository() = default;

Material* MaterialRepository::create(Identifier id, ResourceHandle<ShaderResource> shader)
{
    auto [index, ptr] = m_impl->m_nodes.construct(Material{ id, shader });
    ptr->self_index   = index;
    return &ptr->material;
}

void MaterialRepository::destroy(const Material* handle)
{
    auto* p_node = reinterpret_cast<const MaterialNode*>(handle); // NOLINT
    m_impl->m_nodes.destroy(p_node->self_index);
}

} // namespace Mg::gfx
