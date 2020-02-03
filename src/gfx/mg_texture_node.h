//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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
