//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture_pool.h
 * Creates, stores, and updates texturees.
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg {
class Identifier;
class TextureResource;
} // namespace Mg

namespace Mg::gfx {

class Texture2D;
struct TextureSettings;
struct RenderTargetParams; // Defined in mg_texture_related_types.h
struct TexturePoolData;

class TexturePool : PImplMixin<TexturePoolData> {
public:
    TexturePool();
    ~TexturePool();

    MG_MAKE_NON_MOVABLE(TexturePool);
    MG_MAKE_NON_COPYABLE(TexturePool);

    Texture2D* create(const TextureResource& resource, const TextureSettings& settings);

    Texture2D* create_render_target(const RenderTargetParams& params);

    // Null if there is no texture with that id.
    Texture2D* get(const Identifier& texture_id) const;

    /** Update the texture that was created from resource.
     * N.B. used for hot-reloading of texture files.
     */
    void update(const TextureResource& resource);

    void destroy(Texture2D* texture);

    enum class DefaultTexture { White, Black, Transparent, NormalsFlat, Checkerboard };

    Texture2D* get_default_texture(DefaultTexture type);
};

} // namespace Mg::gfx
