//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
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

struct MaterialPool::Impl {
    std::shared_ptr<TexturePool> texture_pool;
    plf::colony<Material> materials;
};

template<typename MaterialsT> auto find_impl(MaterialsT& materials, Identifier id)
{
    auto it = find_if(materials, [&](const Material& m) { return m.id() == id; });
    return it == materials.end() ? nullptr : &*it;
}

MaterialPool::MaterialPool(std::shared_ptr<TexturePool> texture_pool)
{
    m_impl->texture_pool = std::move(texture_pool);
}

Material* MaterialPool::create(Identifier id, ResourceHandle<ShaderResource> shader_resource_handle)
{
    if (find(id) != nullptr) {
        throw RuntimeError{ "A material with id '{}' already exists.", id.str_view() };
    }
    const auto it = m_impl->materials.emplace(Material{ id, shader_resource_handle });
    return &(*it);
}

namespace {

void init_material_from_resource(Material& material,
                                 const MaterialResource& material_resource,
                                 TexturePool& texture_pool)
{
    try {
        for (const auto& [option, enabled] : material_resource.options()) {
            material.set_option(option, enabled);
        }

        for (const auto& parameter : material_resource.parameters()) {
            material.set_parameter(parameter.name, parameter.default_value);
        }

        for (const auto& sampler : material_resource.samplers()) {
            if (sampler.texture_resource_id) {
                Texture2D* texture = texture_pool.load(*sampler.texture_resource_id);
                MG_ASSERT(texture);
                material.set_sampler(sampler.name, texture);
            }
        }
    }
    catch (const RuntimeError&) {
        log.message(
            "Error initializing Material from MaterialResource '{}'. Is there a mismatch between "
            "the paramaters of the shader and the values defined in the material resource file?",
            material_resource.resource_id().str_view());
        throw;
    }
}

} // namespace

const Material* MaterialPool::get_or_create(const MaterialResource& material_resource)
{
    auto shader_resource = material_resource.shader_resource();
    const Identifier id = material_resource.resource_id();
    if (const Material* preexisting = find(id); preexisting) {
        return preexisting;
    }
    Material* material = create(id, shader_resource);
    init_material_from_resource(*material, material_resource, *m_impl->texture_pool);
    return material;
}

Material* MaterialPool::copy(Identifier id, const Material* source)
{
    MG_ASSERT(source != nullptr);
    auto it = m_impl->materials.emplace(*source);
    it->set_id(id);
    return &(*it);
}

void MaterialPool::update(const MaterialResource& material_resource)
{
    const Identifier material_id = material_resource.resource_id();
    Material* destination = find_impl(m_impl->materials, material_id);

    if (!destination) {
        // No Material has been created from this resource, so there is nothing to update.
        return;
    }

    Opt<Material> new_material;

    try {
        // Get shader resource from material_resource (potentially different from before).
        auto shader_resource = material_resource.shader_resource();

        // Create new Material.
        new_material.emplace(material_resource.resource_id(), shader_resource);
        init_material_from_resource(*new_material, material_resource, *m_impl->texture_pool);
    }
    catch (Mg::RuntimeError&) {
        log.warning("Failed to update Material '{}'.", material_id.str_view());
    }

    // Swap into place.
    MG_ASSERT(new_material.has_value());
    std::swap(*destination, *new_material);
}

void MaterialPool::destroy(const Material* handle)
{
    Material* non_const_ptr = const_cast<Material*>(handle); // NOLINT
    m_impl->materials.erase(m_impl->materials.get_iterator(non_const_ptr));
}

const Material* MaterialPool::find(Identifier id) const
{
    return find_impl(m_impl->materials, id);
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
    return get_all_materials_impl(m_impl->materials);
}

Array<const Material*> MaterialPool::get_all_materials() const
{
    return get_all_materials_impl(m_impl->materials);
}

} // namespace Mg::gfx
