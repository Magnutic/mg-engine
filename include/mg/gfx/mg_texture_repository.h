//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture_repository.h
 * Creates, stores, and updates texturees.
 */

#pragma once

#include "mg/gfx/mg_texture_handle.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg {
class TextureResource;
}

namespace Mg::gfx {

struct RenderTargetParams; // Defined in mg_texture_related_types.h
struct TextureRepositoryData;

class TextureRepository : PImplMixin<TextureRepositoryData> {
public:
    MG_MAKE_NON_MOVABLE(TextureRepository);
    MG_MAKE_NON_COPYABLE(TextureRepository);

    TextureRepository();
    ~TextureRepository();

    TextureHandle create(const TextureResource& resource);

    TextureHandle create_render_target(const RenderTargetParams& params);

    /** Update the texture that was created from resource.
     * N.B. used for hot-reloading of texture files.
     */
    void update(const TextureResource& resource);

    void destroy(TextureHandle handle);
};

} // namespace Mg::gfx
