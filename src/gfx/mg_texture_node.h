//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_texture_node.h
 * Internal texture structure. @see TextureRepository
 */

#pragma once

#include "mg/gfx/mg_texture_handle.h"
#include "mg/utils/mg_assert.h"

#include <cstring>

namespace Mg::gfx {
class Texture2D;
}

namespace Mg::gfx::internal {

inline TextureHandle make_texture_handle(const Texture2D* texture_2d) noexcept
{
    TextureHandle handle{};
    static_assert(sizeof(handle) == sizeof(texture_2d));
    std::memcpy(&handle, &texture_2d, sizeof(texture_2d));
    return handle;
}

/** Dereference texture handle. */
inline Texture2D& dereference_texture_handle(TextureHandle handle) noexcept
{
    MG_ASSERT(handle != TextureHandle{ 0 });

    Texture2D* texture_2d;
    static_assert(sizeof(handle) == sizeof(texture_2d));

    std::memcpy(&texture_2d, &handle, sizeof(texture_2d));
    return *texture_2d;
}

} // namespace Mg::gfx::internal
