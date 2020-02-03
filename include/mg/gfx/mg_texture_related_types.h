//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_render_target_params.h
 * Types related to textures, e.g. construction parameter types, texture units.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"

#include <cstdint>

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

inline bool operator==(ImageSize lhs, ImageSize rhs) noexcept
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

inline bool operator!=(ImageSize lhs, ImageSize rhs) noexcept
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

    int32_t width = 0;
    int32_t height = 0;

    int32_t num_mip_levels = 1;

    TextureFilterMode filter_mode{};
    Format texture_format{};
};

} // namespace Mg::gfx
