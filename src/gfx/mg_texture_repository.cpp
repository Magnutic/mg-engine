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
    plf::colony<internal::TextureNode> nodes;

    // Used for looking up a texture node by identifier.

    FlatMap<Identifier, internal::TextureNode*> node_map;
};

TextureRepository::TextureRepository() = default;
TextureRepository::~TextureRepository() = default;

TextureHandle TextureRepository::create(const TextureResource& resource)
{
    const auto it = impl().nodes.emplace(Texture2D::from_texture_resource(resource));
    it->self_index = impl().nodes.get_index_from_iterator(it);

    internal::TextureNode* ptr = std::addressof(*it);
    impl().node_map.insert({ resource.resource_id(), ptr });

    return make_texture_handle(ptr);
}

TextureHandle TextureRepository::create_render_target(const RenderTargetParams& params)
{
    const auto it = impl().nodes.emplace(Texture2D::render_target(params));
    it->self_index = impl().nodes.get_index_from_iterator(it);
    return make_texture_handle(std::addressof(*it));
}

void TextureRepository::update(const TextureResource& resource)
{
    const Identifier resource_id = resource.resource_id();
    const auto it = impl().node_map.find(resource_id);

    // If not found, then we do not have a texture using the updated resource, so ignore.
    if (it == impl().node_map.end()) {
        return;
    }

    internal::TextureNode* p_node = it->second;
    Texture2D new_texture = Texture2D::from_texture_resource(resource);

    std::swap(p_node->texture, new_texture);

    g_log.write_verbose(
        fmt::format("TextureRepository::update(): Updated {}", resource_id.str_view()));
}

void TextureRepository::destroy(TextureHandle handle)
{
    const auto& texture_node = internal::texture_node(handle);
    const auto it = impl().nodes.get_iterator_from_index(texture_node.self_index);
    const Identifier texture_id = it->texture.id();
    impl().nodes.erase(it);
    impl().node_map.erase(texture_id);
}

} // namespace Mg::gfx
