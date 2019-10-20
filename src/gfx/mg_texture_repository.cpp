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

#include "mg/gfx/mg_texture_repository.h"

#include "mg/containers/mg_pooling_vector.h"
#include "mg/core/mg_log.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resources/mg_texture_resource.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_texture_node.h"

#include <fmt/core.h>

#include <vector>

namespace Mg::gfx {

struct TextureRepositoryData {
    // Size of the pools allocated for the data structure, in number of elements.
    // This is a fairly arbitrary choice: larger pools may make allocations more rare and provide
    // better data locality but could also waste space if the pool is never filled.
    static constexpr size_t k_texture_node_pool_size = 512;

    // Texture node storage -- stores elements largely contiguously, but does not invalidate
    // pointers.
    PoolingVector<internal::TextureNode> nodes{ k_texture_node_pool_size };

    // Used for looking up a texture node by identifier.
    // TODO: lookup can be optimised by storing in sorted order. Use std::lower_bound.
    std::vector<std::pair<Identifier, internal::TextureNode*>> node_map;
};

TextureRepository::TextureRepository()  = default;
TextureRepository::~TextureRepository() = default;

TextureHandle TextureRepository::create(const TextureResource& resource)
{
    const auto [index, ptr] = impl().nodes.construct(Texture2D::from_texture_resource(resource));
    ptr->self_index   = index;

    impl().node_map.emplace_back(resource.resource_id(), ptr);

    return make_texture_handle(ptr);
}

TextureHandle TextureRepository::create_render_target(const RenderTargetParams& params)
{
    const auto [index, ptr] = impl().nodes.construct(Texture2D::render_target(params));
    ptr->self_index   = index;
    return make_texture_handle(ptr);
}

void TextureRepository::update(const TextureResource& resource)
{
    const Identifier resource_id = resource.resource_id();
    const auto it = find_if(impl().node_map, [&](auto& pair) { return pair.first == resource_id; });

    // If not found, then we do not have a texture using the updated resource, so ignore.
    if (it == impl().node_map.end()) { return; }

    internal::TextureNode* p_node      = it->second;
    Texture2D              new_texture = Texture2D::from_texture_resource(resource);

    std::swap(p_node->texture, new_texture);

    g_log.write_verbose(
        fmt::format("TextureRepository::update(): Updated {}", resource_id.str_view()));
}

void TextureRepository::destroy(TextureHandle handle)
{
    const auto& texture_node = internal::texture_node(handle);
    impl().nodes.destroy(texture_node.self_index);

    const auto it = find_if(impl().node_map,
                            [&](auto& pair) { return pair.second == &texture_node; });
    if (it != impl().node_map.end()) { impl().node_map.erase(it); }
}

} // namespace Mg::gfx
