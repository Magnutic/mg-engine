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
#include "mg/gfx/mg_texture_cube.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resource_cache/mg_resource_cache.h"
#include "mg/resource_cache/mg_resource_exceptions.h"
#include "mg/resources/mg_texture_resource.h"
#include "mg/utils/mg_string_utils.h"

#include <plf_colony.h>

#include <fmt/core.h>

#include <type_traits>
#include <variant>

namespace Mg::gfx {

namespace {

// Information about a texture within the pool.
struct TextureNode {
    std::variant<Texture2D*, TextureCube*> instance{};
    TextureSettings settings{};
};

} // namespace

using TexturesById = FlatMap<Identifier, TextureNode, Identifier::HashCompare>;

struct TexturePool::Impl {
    // The resource cache from which we will load textures.
    std::shared_ptr<ResourceCache> resource_cache;

    // Texture node storage -- stores elements largely contiguously, but does not invalidate
    // pointers.
    plf::colony<Texture2D> textures;
    plf::colony<TextureCube> cubemaps;

    template<typename TextureT> plf::colony<TextureT>& storage_for()
    {
        if constexpr (std::is_same_v<TextureT, Texture2D>) {
            return textures;
        }
        else {
            return cubemaps;
        }
    }

    template<typename TextureT> const plf::colony<TextureT>& storage_for() const
    {
        return const_cast<Impl&>(*this).storage_for<TextureT>(); // NOLINT
    }

    // Used for looking up a texture node by identifier.
    TexturesById textures_by_id;
};

namespace {

// Deduce texture settings given a TextureResource.
// Based on a system of convention where the filename suffix indicates its intended use.
TextureSettings deduce_texture_settings(const TextureResource& texture_resource)
{
    TextureSettings settings = {};
    const Identifier id = texture_resource.resource_id();

    Opt<TextureCategory> category = deduce_texture_category(id.str_view());
    if (!category.has_value()) {
        log.warning("Could not deduce texture category for '{}'", id.str_view());
        log.message("Note: expected filename ending with one of the following:");
        for (const auto& [c, suffix] : g_texture_category_to_filename_suffix_map) {
            log.message("\t{}", suffix);
        }
    }
    else {
        switch (category.value()) {
        case TextureCategory::Diffuse:
            settings.sRGB = SRGBSetting::sRGB;
            settings.dxt1_has_alpha = false;
            break;
        case TextureCategory::Diffuse_transparent:
            settings.sRGB = SRGBSetting::sRGB;
            settings.dxt1_has_alpha = true;
            break;
        case TextureCategory::Diffuse_alpha:
            settings.sRGB = SRGBSetting::sRGB;
            settings.dxt1_has_alpha = true;
            break;
        case TextureCategory::Normal:
            settings.sRGB = SRGBSetting::Linear;
            break;
        case TextureCategory::Specular_gloss:
            settings.sRGB = SRGBSetting::sRGB;
            break;
        case TextureCategory::Ao_roughness_metallic:
            settings.sRGB = SRGBSetting::Linear;
            break;
        }
    }

    return settings;
}

TexturesById::iterator try_insert_into_id_map(TexturesById& textures_by_id, Identifier key)
{
    const auto [map_it, inserted] = textures_by_id.insert({ key, TextureNode{} });
    if (inserted) {
        return map_it;
    }
    throw RuntimeError{ "Creating texture {}: a texture by that identifier already exists.",
                        key.str_view() };
}

template<typename TextureT>
TextureT* create_texture_impl(TexturePool::Impl& data,
                              const Identifier id,
                              const std::function<TextureT()>& texture_create_func,
                              const TextureSettings& settings)
{
    auto& texture_storage = data.storage_for<TextureT>();
    TextureT& texture = *texture_storage.emplace(texture_create_func());

    const auto id_map_it = try_insert_into_id_map(data.textures_by_id, id);
    id_map_it->second.instance = &texture;
    id_map_it->second.settings = settings;
    return &texture;
}

template<typename TextureT>
TextureT* get_impl(const TexturePool::Impl& data, const Identifier& texture_id)
{
    const auto it = data.textures_by_id.find(texture_id);
    if (it == data.textures_by_id.end()) {
        return nullptr;
    }
    const TextureNode& node = it->second;
    if (std::holds_alternative<TextureT*>(node.instance)) {
        return std::get<TextureT*>(node.instance);
    }
    return nullptr;
}

template<typename TextureT>
TextureT* from_resource_impl(TexturePool::Impl& data,
                             const TextureResource& resource,
                             const TextureSettings& settings)
{
    auto generate_texture = [&resource, &settings] {
        return TextureT::from_texture_resource(resource, settings);
    };
    return create_texture_impl<TextureT>(data, resource.resource_id(), generate_texture, settings);
}

template<typename TextureT>
TextureT* load_impl(TexturePool::Impl& data, const Identifier& texture_id)
{
    if (auto* result = get_impl<TextureT>(data, texture_id); result) {
        return result;
    }

    try {
        auto access_guard = data.resource_cache->access_resource<TextureResource>(texture_id);
        const auto settings = deduce_texture_settings(*access_guard);
        return from_resource_impl<TextureT>(data, *access_guard, settings);
    }
    catch (const ResourceNotFound&) {
        return nullptr;
    }
}

template<typename TextureT> void destroy_impl(TexturePool::Impl& data, TextureT* texture)
{
    auto& texture_storage = data.storage_for<TextureT>();
    const auto it = texture_storage.get_iterator(texture);
    const Identifier texture_id = it->id();
    texture_storage.erase(it);
    data.textures_by_id.erase(texture_id);
}

} // namespace

TexturePool::TexturePool(std::shared_ptr<ResourceCache> resource_cache)
{
    MG_ASSERT(resource_cache != nullptr);
    m_impl->resource_cache = std::move(resource_cache);
}

TexturePool::~TexturePool() = default;

Texture2D* TexturePool::load_texture2d(const Identifier& texture_id)
{
    return load_impl<Texture2D>(*m_impl, texture_id);
}

TextureCube* TexturePool::load_cubemap(const Identifier& texture_id)
{
    return load_impl<TextureCube>(*m_impl, texture_id);
}

Texture2D* TexturePool::create_render_target(const RenderTargetParams& params)
{
    MG_GFX_DEBUG_GROUP("TexturePool::create_render_target")
    auto generate_texture = [&params] { return Texture2D::render_target(params); };
    return create_texture_impl<Texture2D>(*m_impl, params.render_target_id, generate_texture, {});
}

Texture2D* TexturePool::get_texture2d(const Identifier& texture_id) const
{
    return get_impl<Texture2D>(*m_impl, texture_id);
}

void TexturePool::update(const TextureResource& resource)
{
    MG_GFX_DEBUG_GROUP("TexturePool::update")
    const Identifier resource_id = resource.resource_id();
    const auto it = m_impl->textures_by_id.find(resource_id);

    // If not found, then we do not have a texture using the updated resource, so ignore.
    if (it == m_impl->textures_by_id.end()) {
        return;
    }

    TextureNode& node = it->second;

    std::visit(
        [&]<typename TextureT>(TextureT* texture) {
            *texture = TextureT::from_texture_resource(resource, node.settings);
        },
        node.instance);

    log.verbose("TexturePool::update(): Updated {}", resource_id.str_view());
}

void TexturePool::destroy(Texture2D* texture)
{
    MG_GFX_DEBUG_GROUP("TexturePool::destroy")
    destroy_impl<Texture2D>(*m_impl, texture);
}

void TexturePool::destroy(TextureCube* texture)
{
    MG_GFX_DEBUG_GROUP("TexturePool::destroy")
    destroy_impl<TextureCube>(*m_impl, texture);
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

    if (Texture2D* texture = get_texture2d(id)) {
        return texture;
    }

    auto generate_texture = [&]() {
        std::span<const uint8_t> buffer = default_texture_buffers.at(index);
        return Texture2D::from_rgba8_buffer(id, buffer, 2, 2, settings);
    };

    return create_texture_impl<Texture2D>(*m_impl, id, generate_texture, settings);
};

} // namespace Mg::gfx
