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

/** @file mg_texture2d.h
 * Constructs and owns an instance of a 2D texture in the graphics context.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_opaque_handle.h"

#include <cstdint>

namespace Mg {
class TextureResource;
}

namespace Mg::gfx {


/** Constructs and owns the graphics API's texture object from a TextureResource resource. */
class Texture2D {
public:
    /** Create a texture from the given resource. */
    static Texture2D from_texture_resource(const TextureResource& texture_resource);

    /** Create a render-target texture. */
    static Texture2D render_target(const RenderTargetParams& params);

    MG_MAKE_DEFAULT_MOVABLE(Texture2D);
    MG_MAKE_NON_COPYABLE(Texture2D);

    ~Texture2D() { unload(); }

    ImageSize image_size() const noexcept { return m_image_size; }

    Identifier id() const noexcept { return m_id; }

    OpaqueHandle::Value gfx_api_handle() const { return m_gfx_api_handle.value; }

private:
    Texture2D(OpaqueHandle&& gfx_api_handle) : m_gfx_api_handle(std::move(gfx_api_handle)) {}

    void unload();

    OpaqueHandle m_gfx_api_handle;

    ImageSize m_image_size{};

    Identifier m_id{ "" };
};

} // namespace Mg::gfx
