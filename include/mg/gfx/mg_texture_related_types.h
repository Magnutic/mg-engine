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

/** @file mg_render_target_params.h
 * Types related to textures, e.g. construction parameter types, texture units.
 */

#pragma once

#include <cstdint>

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"

namespace Mg::gfx {

/** TextureUnit values may be at most this large. */
static constexpr size_t k_max_texture_unit = 15;

/** TextureUnit -- target to which a sampler may be bound. */
class TextureUnit {
public:
    constexpr explicit TextureUnit(uint32_t unit) : m_unit(unit)
    {
        MG_ASSERT(unit <= k_max_texture_unit);
    }

    constexpr uint32_t get() const noexcept { return m_unit; }

private:
    uint32_t m_unit = 0;
};

struct ImageSize {
    int32_t width{};
    int32_t height{};
};

inline bool operator==(ImageSize lhs, ImageSize rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

inline bool operator!=(ImageSize lhs, ImageSize rhs)
{
    return !(lhs == rhs);
}

/** Texture sampling filtering methods. */
enum class TextureFilterMode {
    Nearest, /** Nearest-neighbour filtering. */
    Linear   /** Linearly interpolated -- smooth -- filtering. */
};
/** Input parameters for creating render-target textures. */
struct RenderTargetParams {
    enum class Format {
        RGBA8,   /** Red/Green/Blue/Alpha channels of 8-bit unsigned int */
        RGBA16F, /** Red/Green/Blue/Alpha channels of 16-bit float */
        RGBA32F, /** Red/Green/Blue/Alpha channels of 32-bit float */
        Depth24, /** 24-bit depth */
    };

    Identifier render_target_id{ "<anonymous render target texture>" };

    int32_t width{};
    int32_t height{};

    TextureFilterMode filter_mode{};
    Format            texture_format{};
};

} // namespace Mg::gfx
