//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_texture_pool.h"

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

namespace Mg::gfx {

namespace {

// Information about a texture within the pool.
struct TextureNode {
    Texture2D* instance;
    TextureSettings settings;
};

} // namespace

struct TexturePoolData {
    // Texture node storage -- stores elements largely contiguously, but does not invalidate
    // pointers.
    plf::colony<Texture2D> textures;

    // Used for looking up a texture node by identifier.
    using HandleMap = FlatMap<Identifier, TextureNode, Identifier::HashCompare>;
    HandleMap texture_map;
};

namespace {

TexturePoolData::HandleMap::iterator try_insert_into_handle_map(TexturePoolData& data,
                                                                Identifier key)
{
    const auto [map_it, inserted] = data.texture_map.insert({ key, { nullptr, {} } });
    if (inserted) {
        return map_it;
    }
    const auto fmt_string = "Creating texture {}: a texture by that identifier already exists.";
    const auto error_msg = fmt::format(fmt_string, key.str_view());
    log.error(error_msg);
    throw RuntimeError{};
}

Texture2D* create_texture_impl(TexturePoolData& data,
                               const Identifier id,
                               const std::function<Texture2D()>& texture_create_func,
                               const TextureSettings& settings)
{
    const auto handle_map_it = try_insert_into_handle_map(data, id);
    const auto texture_it = data.textures.emplace(texture_create_func());
    Texture2D* ptr = &*texture_it;
    handle_map_it->second.instance = ptr;
    handle_map_it->second.settings = settings;
    return ptr;
}

} // namespace

TexturePool::TexturePool() = default;
TexturePool::~TexturePool() = default;

Texture2D* TexturePool::create(const TextureResource& resource, const TextureSettings& settings)
{
    MG_GFX_DEBUG_GROUP("TexturePool::create")
    auto generate_texture = [&resource, &settings] {
        return Texture2D::from_texture_resource(resource, settings);
    };
    return create_texture_impl(impl(), resource.resource_id(), generate_texture, settings);
}

Texture2D* TexturePool::create_render_target(const RenderTargetParams& params)
{
    MG_GFX_DEBUG_GROUP("TexturePool::create_render_target")
    auto generate_texture = [&params] { return Texture2D::render_target(params); };
    return create_texture_impl(impl(), params.render_target_id, generate_texture, {});
}

Texture2D* TexturePool::get(const Identifier& texture_id) const
{
    const auto it = impl().texture_map.find(texture_id);
    if (it == impl().texture_map.end()) {
        return nullptr;
    }
    return it->second.instance;
}

void TexturePool::update(const TextureResource& resource)
{
    MG_GFX_DEBUG_GROUP("TexturePool::update")
    const Identifier resource_id = resource.resource_id();
    const auto it = impl().texture_map.find(resource_id);

    // If not found, then we do not have a texture using the updated resource, so ignore.
    if (it == impl().texture_map.end()) {
        return;
    }

    Texture2D& old_texture = *it->second.instance;
    Texture2D new_texture = Texture2D::from_texture_resource(resource, it->second.settings);

    std::swap(old_texture, new_texture);

    log.verbose("TexturePool::update(): Updated {}", resource_id.str_view());
}

void TexturePool::destroy(Texture2D* texture)
{
    MG_GFX_DEBUG_GROUP("TexturePool::destroy")
    const auto it = impl().textures.get_iterator_from_pointer(texture);
    const Identifier texture_id = it->id();
    impl().textures.erase(it);
    impl().texture_map.erase(texture_id);
}

namespace {

const auto num_default_textures = static_cast<size_t>(TexturePool::DefaultTexture::Checkerboard) +
                                  1;

constexpr std::array<Identifier, num_default_textures> default_texture_identifiers = { {
    "__default_texture_rgba_white",
    "__default_texture_rgba_black",
    "__default_texture_rgba_transparent",
    "__default_texture_normals_flat",
    "__default_texture_checkerboard",
} };

// clang-format off
constexpr std::array<std::array<uint8_t, 16>, num_default_textures> default_texture_buffers = {{
    { // White
         255, 255, 255, 255,
         255, 255, 255, 255,
         255, 255, 255, 255,
         255, 255, 255, 255,
    },
    { // Black
         0, 0, 0, 255,
         0, 0, 0, 255,
         0, 0, 0, 255,
         0, 0, 0, 255,
    },
    { // Transparent
         0, 0, 0, 0,
         0, 0, 0, 0,
         0, 0, 0, 0,
         0, 0, 0, 0,
    },
    { // Normals flat
         127, 127, 255, 255,
         127, 127, 255, 255,
         127, 127, 255, 255,
         127, 127, 255, 255,
    },
    { // Checkerboard
         0, 0, 0, 255,
         255, 255, 255, 255,
         255, 255, 255, 255,
         0, 0, 0, 255,
    }
}};
// clang-format on

} // namespace

Texture2D* TexturePool::get_default_texture(DefaultTexture type)
{
    const auto index = static_cast<size_t>(type);
    const Identifier& id = default_texture_identifiers.at(index);

    TextureSettings settings = {};
    settings.sRGB = type == DefaultTexture::NormalsFlat ? SRGBSetting::Linear
                                                        : SRGBSetting::Default;
    settings.filtering = Filtering::Nearest;

    if (Texture2D* texture = get(id)) {
        return texture;
    }

    auto generate_texture = [&]() {
        span<const uint8_t> buffer = default_texture_buffers.at(index);
        return Texture2D::from_rgba8_buffer(id, buffer, 2, 2, settings);
    };

    return create_texture_impl(impl(), id, generate_texture, settings);
};

} // namespace Mg::gfx
