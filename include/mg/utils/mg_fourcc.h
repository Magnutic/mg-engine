//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_fourcc.h
 * Helper functions for dealing with four-character codes (filetype identifiers).
 */

#pragma once

#include "mg/utils/mg_assert.h"

#include <cstdint>
#include <span>

namespace Mg {

constexpr uint32_t make_fourcc(const std::span<const char> four_chars)
{
    MG_ASSERT(four_chars.size() == 4 || (four_chars.size() == 5 && four_chars.back() == '\0'));
    uint32_t out = 0u;
    out |= (static_cast<uint32_t>(four_chars[0]));
    out |= (static_cast<uint32_t>(four_chars[1]) << 8u);
    out |= (static_cast<uint32_t>(four_chars[2]) << 16u);
    out |= (static_cast<uint32_t>(four_chars[3]) << 24u);
    return out;
}

constexpr std::array<char, 4> decompose_fourcc(const uint32_t fourcc)
{
    std::array<char, 4> out{};
    out[0] = static_cast<char>(fourcc & 0xff);
    out[1] = static_cast<char>((fourcc >> 8u) & 0xff);
    out[2] = static_cast<char>((fourcc >> 16u) & 0xff);
    out[3] = static_cast<char>((fourcc >> 24u) & 0xff);
    return out;
}

static_assert(make_fourcc("TEST") == 0x54534554);
static_assert(decompose_fourcc(0x54534554) == std::array<char, 4>{ 'T', 'E', 'S', 'T' });

} // namespace Mg
