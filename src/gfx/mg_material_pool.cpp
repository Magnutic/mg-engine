//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_material_pool.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_material.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_texture_pool.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_material_resource.h"
#include "mg/utils/mg_stl_helpers.h"

#include <plf_colony.h>

#include <cstring> // memcpy

namespace Mg::gfx {

struct MaterialPoolData {
    std::shared_ptr<TexturePool> texture_pool;
    plf::colony<Material> materials;
};

MaterialPool::MaterialPool(std::shared_ptr<TexturePool> texture_pool)
{
    impl().texture_pool = std::move(texture_pool);
}

MaterialPool::~MaterialPool() = default;

Material* MaterialPool::create(Identifier id, ResourceHandle<ShaderResource> shader_resource_handle)
{
    const auto it = impl().materials.emplace(Material{ id, shader_resource_handle });
    return &(*it);
}

namespace {
void init_material_from_resource(Material& material,
                                 const MaterialResource& material_resource,
                                 TexturePool& texture_pool)
{
    try {
        // Init options
        for (const auto& [option, enabled] : material_resource.options()) {
            material.set_option(option, enabled);
        }

        // Init parameters
        for (const material::Parameter& parameter : material_resource.parameters()) {
            material.set_parameter(parameter.name, parameter.value);
        }

        // Init samplers
        for (const material::Sampler& sampler : material_resource.samplers()) {
            Texture2D* texture = texture_pool.load(sampler.texture_resource_id);
            MG_ASSERT(texture);
            material.set_sampler(sampler.name, texture->handle(), sampler.texture_resource_id);
        }
    }
    catch (const RuntimeError&) {
        log.message(
            "Error initializing Material from MaterialResource '{}'. Is there a mismatch between "
            "shader parameters and material values?",
            material_resource.resource_id().str_view());
        throw;
    }
}
} // namespace

Material* MaterialPool::create(const MaterialResource& material_resource)
{
    auto shader_resource = material_resource.shader_resource();
    const Identifier id = material_resource.resource_id();
    Material* material = create(id, shader_resource);
    init_material_from_resource(*material, material_resource, *impl().texture_pool);
    return material;
}

void MaterialPool::update(const MaterialResource& material_resource) {

    const Identifier material_id = material_resource.resource_id();
    Material* destination = find(material_id);

    if (!destination)
    {
        return;
    }

    // Get shader resource from material_resource (potentially different from before).
    auto shader_resource = material_resource.shader_resource();

    // Create new Material.
    Material new_material{material_resource.resource_id(), shader_resource};
    init_material_from_resource(new_material, material_resource, *impl().texture_pool);

    // Swap into place.
    std::swap(*destination, new_material);
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
