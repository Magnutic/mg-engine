//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture_repository.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_identifier.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resources/mg_texture_resource.h"

#include <plf_colony.h>

#include <fmt/core.h>

#include <vector>

namespace Mg::gfx {

struct TextureRepositoryData {
    // Texture node storage -- stores elements largely contiguously, but does not invalidate
    // pointers.
    plf::colony<Texture2D> textures;

    // Used for looking up a texture node by identifier.
    using HandleMap = FlatMap<Identifier, Texture2D*, Identifier::HashCompare>;
    HandleMap texture_map;
};

namespace {

TextureRepositoryData::HandleMap::iterator try_insert_into_handle_map(TextureRepositoryData& data,
                                                                      Identifier key)
{
    const auto [map_it, inserted] = data.texture_map.insert({ key, nullptr });
    if (inserted) {
        return map_it;
    }
    const auto fmt_string = "Creating texture {}: a texture by that identifier already exists.";
    const auto error_msg = fmt::format(fmt_string, key.str_view());
    log.error(error_msg);
    throw RuntimeError{};
}

Texture2D* create_texture_impl(TextureRepositoryData& data,
                               const Identifier id,
                               const std::function<Texture2D()>& texture_create_func)
{
    const auto handle_map_it = try_insert_into_handle_map(data, id);
    const auto texture_it = data.textures.emplace(texture_create_func());
    Texture2D* ptr = &*texture_it;
    handle_map_it->second = ptr;
    return ptr;
}

} // namespace

TextureRepository::TextureRepository() = default;
TextureRepository::~TextureRepository() = default;

Texture2D* TextureRepository::create(const TextureResource& resource)
{
    MG_GFX_DEBUG_GROUP("TextureRepository::create")
    auto generate_texture = [&resource] { return Texture2D::from_texture_resource(resource); };
    return create_texture_impl(impl(), resource.resource_id(), generate_texture);
}

Texture2D* TextureRepository::create_render_target(const RenderTargetParams& params)
{
    MG_GFX_DEBUG_GROUP("TextureRepository::create_render_target")
    auto generate_texture = [&params] { return Texture2D::render_target(params); };
    return create_texture_impl(impl(), params.render_target_id, generate_texture);
}

Texture2D* TextureRepository::get(const Identifier& texture_id) const
{
    const auto it = impl().texture_map.find(texture_id);
    if (it == impl().texture_map.end()) {
        return nullptr;
    }
    return it->second;
}

void TextureRepository::update(const TextureResource& resource)
{
    MG_GFX_DEBUG_GROUP("TextureRepository::update")
    const Identifier resource_id = resource.resource_id();
    const auto it = impl().texture_map.find(resource_id);

    // If not found, then we do not have a texture using the updated resource, so ignore.
    if (it == impl().texture_map.end()) {
        return;
    }

    Texture2D& old_texture = *it->second;
    Texture2D new_texture = Texture2D::from_texture_resource(resource);

    std::swap(old_texture, new_texture);

    log.verbose(
        fmt::format("TextureRepository::update(): Updated {}", resource_id.str_view()));
}

void TextureRepository::destroy(Texture2D* texture)
{
    MG_GFX_DEBUG_GROUP("TextureRepository::destroy")
    const auto it = impl().textures.get_iterator_from_pointer(texture);
    const Identifier texture_id = it->id();
    impl().textures.erase(it);
    impl().texture_map.erase(texture_id);
}

} // namespace Mg::gfx
