//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2023, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture.h
 * Owning handle for a texture in the graphics context.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/utils/mg_macros.h"

#include <cstdint>

namespace Mg::gfx {

/** Owning handle for a texture in the graphics context. */
class Texture {
public:
    MG_MAKE_DEFAULT_MOVABLE(Texture);
    MG_MAKE_NON_COPYABLE(Texture);

    ~Texture() { unload(); }

    ImageSize image_size() const noexcept { return m_image_size; }

    Identifier id() const noexcept { return m_id; }

    TextureHandle handle() const noexcept { return m_handle; }

private:
    explicit Texture(TextureHandle&& handle) noexcept : m_handle(std::move(handle)) {}

    void unload() noexcept;

    TextureHandle m_handle;

    ImageSize m_image_size{};

    Identifier m_id{ "" };
};

} // namespace Mg::gfx
