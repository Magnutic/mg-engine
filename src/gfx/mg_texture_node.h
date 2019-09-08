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

/** @file mg_texture_node.h
 * Internal texture structure. @see TextureRepository
 */

#pragma once

#include "mg/gfx/mg_texture2d.h"
#include "mg/gfx/mg_texture_handle.h"
#include "mg/utils/mg_assert.h"

#include <cstring>
#include <utility>

namespace Mg::gfx::internal {

struct TextureNode {
    explicit TextureNode(Texture2D&& tex) noexcept : texture(std::move(tex)) {}

    Texture2D texture;

    // Index of this object in data structure -- used for deletion.
    uint32_t self_index{};
};

inline TextureHandle make_texture_handle(const TextureNode* texture_node) noexcept
{
    TextureHandle handle{};
    static_assert(sizeof(handle) >= sizeof(texture_node));
    std::memcpy(&handle, &texture_node, sizeof(texture_node));
    return handle;
}

/** Dereference texture handle. */
inline const TextureNode& texture_node(TextureHandle handle) noexcept
{
    MG_ASSERT(handle != TextureHandle{ 0 });
    return *reinterpret_cast<const TextureNode*>(handle); // NOLINT
}

} // namespace Mg::gfx::internal
