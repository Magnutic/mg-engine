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

    explicit TextureRepository();
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
