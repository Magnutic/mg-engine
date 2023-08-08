//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture_pool.h
 * Creates, stores, and updates texturees.
 */

#pragma once

#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"

#include <memory>

namespace Mg {
class Identifier;
class ResourceCache;
class TextureResource;
} // namespace Mg

namespace Mg::gfx {

class Texture2D;
class TextureCube;
struct TextureSettings;
struct RenderTargetParams; // Defined in mg_texture_related_types.h

class TexturePool {
public:
    explicit TexturePool(std::shared_ptr<ResourceCache> resource_cache);
    ~TexturePool();

    MG_MAKE_NON_MOVABLE(TexturePool);
    MG_MAKE_NON_COPYABLE(TexturePool);

    /** Loads texture from resource cache unless it is already in the pool. */
    Texture2D* load_texture2d(const Identifier& texture_id);

    /** Loads texture from resource cache unless it is already in the pool. */
    TextureCube* load_cubemap(const Identifier& texture_id);

    Texture2D* from_resource(const TextureResource& resource, const TextureSettings& settings);

    TextureCube* from_resource_cubemap(const TextureResource& resource,
                                       const TextureSettings& settings);

    Texture2D* create_render_target(const RenderTargetParams& params);

    /** Find a texture by its resource id. Null if there is no Texture2D with that id. */
    Texture2D* get_texture2d(const Identifier& texture_id) const;

    /** Find a texture by its resource id. Null if there is no TextureCube with that id. */
    TextureCube* get_cubemap(const Identifier& texture_id) const;

    /** Update the texture that was created from resource.
     * Used for hot-reloading of texture files.
     */
    void update(const TextureResource& resource);

    void destroy(Texture2D* texture);

    void destroy(TextureCube* texture);

    enum class DefaultTexture { White, Black, Transparent, NormalsFlat, Checkerboard };

    Texture2D* get_default_texture(DefaultTexture type);

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

} // namespace Mg::gfx
