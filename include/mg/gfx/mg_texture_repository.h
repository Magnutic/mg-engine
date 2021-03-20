//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture_repository.h
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

struct RenderTargetParams; // Defined in mg_texture_related_types.h
struct TextureRepositoryData;

class TextureRepository : PImplMixin<TextureRepositoryData> {
public:
    TextureRepository();
    ~TextureRepository();

    MG_MAKE_NON_MOVABLE(TextureRepository);
    MG_MAKE_NON_COPYABLE(TextureRepository);

    Texture2D* create(const TextureResource& resource);

    Texture2D* create_render_target(const RenderTargetParams& params);

    // Null if there is no texture with that id.
    Texture2D* get(const Identifier& texture_id) const;

    /** Update the texture that was created from resource.
     * N.B. used for hot-reloading of texture files.
     */
    void update(const TextureResource& resource);

    void destroy(Texture2D* texture);

    enum class DefaultTexture {
        White,
        Black,
        Transparent,
        NormalsFlat,
        Checkerboard
    };

    Texture2D* get_default_texture(DefaultTexture type);
};

} // namespace Mg::gfx
