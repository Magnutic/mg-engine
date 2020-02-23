//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture_repository.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_log.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resources/mg_texture_resource.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_texture_node.h"

#include <plf_colony.h>

#include <fmt/core.h>

#include <vector>

namespace Mg::gfx {

struct TextureRepositoryData {
    // Texture node storage -- stores elements largely contiguously, but does not invalidate
    // pointers.
    plf::colony<Texture2D> gpu_textures;

    // Used for looking up a texture node by identifier.

    FlatMap<Identifier, TextureHandle> texture_map;
};

TextureRepository::TextureRepository() = default;
TextureRepository::~TextureRepository() = default;

TextureHandle TextureRepository::create(const TextureResource& resource)
{
    const auto it = impl().gpu_textures.emplace(Texture2D::from_texture_resource(resource));
    Texture2D* ptr = std::addressof(*it);
    const TextureHandle handle = internal::make_texture_handle(ptr);
    impl().texture_map.insert({ resource.resource_id(), handle });
    return handle;
}

TextureHandle TextureRepository::create_render_target(const RenderTargetParams& params)
{
    const auto it = impl().gpu_textures.emplace(Texture2D::render_target(params));
    Texture2D* ptr = std::addressof(*it);
    const TextureHandle handle = internal::make_texture_handle(ptr);
    impl().texture_map.insert({ params.render_target_id, handle });
    return handle;
}

void TextureRepository::update(const TextureResource& resource)
{
    const Identifier resource_id = resource.resource_id();
    const auto it = impl().texture_map.find(resource_id);

    // If not found, then we do not have a texture using the updated resource, so ignore.
    if (it == impl().texture_map.end()) {
        return;
    }

    Texture2D& old_texture = internal::dereference_texture_handle(it->second);
    Texture2D new_texture = Texture2D::from_texture_resource(resource);

    std::swap(old_texture, new_texture);

    g_log.write_verbose(
        fmt::format("TextureRepository::update(): Updated {}", resource_id.str_view()));
}

void TextureRepository::destroy(TextureHandle handle)
{
    Texture2D* ptr = &internal::dereference_texture_handle(handle);
    const auto it = impl().gpu_textures.get_iterator_from_pointer(ptr);
    const Identifier texture_id = it->id();
    impl().gpu_textures.erase(it);
    impl().texture_map.erase(texture_id);
}

} // namespace Mg::gfx
