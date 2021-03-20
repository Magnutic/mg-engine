//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture2d.h
 * Constructs and owns an instance of a 2D texture in the graphics context.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

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

    /** Create a texture from RGBA8 buffer. */
    // TODO parameters, mipmaps, compression?
    static Texture2D from_rgba8_buffer(Identifier id,
                                       span<const uint8_t> rgba8_buffer,
                                       int32_t width,
                                       int32_t height);

    MG_MAKE_DEFAULT_MOVABLE(Texture2D);
    MG_MAKE_NON_COPYABLE(Texture2D);

    ~Texture2D() { unload(); }

    ImageSize image_size() const noexcept { return m_image_size; }

    Identifier id() const noexcept { return m_id; }

    TextureHandle handle() const noexcept { return m_handle; }

private:
    Texture2D(TextureHandle&& handle) noexcept : m_handle(std::move(handle)) {}

    void unload() noexcept;

    TextureHandle m_handle;

    ImageSize m_image_size{};

    Identifier m_id{ "" };
};

} // namespace Mg::gfx
