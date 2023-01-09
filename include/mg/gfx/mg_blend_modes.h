//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2023, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file renderer/mg_blend_modes.h
 * Blend mode types and predefined constants.
 */

#pragma once

#include <mg/utils/mg_enum.h>

#include <compare>

namespace Hjson {
class Value;
}

/** Pre-defined blend modes. */
namespace Mg::gfx {

MG_ENUM(BlendFactor,
        (zero,
         one,
         src_colour,
         one_minus_src_colour,
         src_alpha,
         one_minus_src_alpha,
         dst_alpha,
         one_minus_dst_alpha,
         dst_colour,
         one_minus_dst_colour))

MG_ENUM(BlendOp, (add, subtract, reverse_subtract, min, max))

struct BlendMode {
    BlendOp colour_blend_op : 3;
    BlendOp alpha_blend_op : 3;
    BlendFactor src_colour_factor : 6;
    BlendFactor dst_colour_factor : 6;
    BlendFactor src_alpha_factor : 6;
    BlendFactor dst_alpha_factor : 6;

    static Hjson::Value serialize(const BlendMode& blend_mode);
    static BlendMode deserialize(const Hjson::Value& v);

    friend std::strong_ordering operator<=>(const BlendMode& l, const BlendMode& r) = default;
};

} // namespace Mg::gfx

namespace Mg::gfx::blend_mode_constants {

/** Default BlendMode */
[[maybe_unused]] constexpr BlendMode bm_default{
    .colour_blend_op = BlendOp::add,
    .alpha_blend_op = BlendOp::add,
    .src_colour_factor = BlendFactor::one,
    .dst_colour_factor = BlendFactor::zero,
    .src_alpha_factor = BlendFactor::one,
    .dst_alpha_factor = BlendFactor::zero,
};

/** Alpha BlendMode */
[[maybe_unused]] constexpr BlendMode bm_alpha{
    .colour_blend_op = BlendOp::add,
    .alpha_blend_op = BlendOp::add,
    .src_colour_factor = BlendFactor::src_alpha,
    .dst_colour_factor = BlendFactor::one_minus_src_alpha,
    .src_alpha_factor = BlendFactor::one,
    .dst_alpha_factor = BlendFactor::one,
};

/** Premultiplied alpha BlendMode */
[[maybe_unused]] constexpr BlendMode bm_alpha_premultiplied{
    .colour_blend_op = BlendOp::add,
    .alpha_blend_op = BlendOp::add,
    .src_colour_factor = BlendFactor::one,
    .dst_colour_factor = BlendFactor::one_minus_src_alpha,
    .src_alpha_factor = BlendFactor::one,
    .dst_alpha_factor = BlendFactor::one,
};

/** Additive BlendMode */
[[maybe_unused]] constexpr BlendMode bm_add{
    .colour_blend_op = BlendOp::add,
    .alpha_blend_op = BlendOp::add,
    .src_colour_factor = BlendFactor::src_alpha,
    .dst_colour_factor = BlendFactor::one,
    .src_alpha_factor = BlendFactor::one,
    .dst_alpha_factor = BlendFactor::one,
};

/** Premultiplied additive BlendMode */
[[maybe_unused]] constexpr BlendMode bm_add_premultiplied{
    .colour_blend_op = BlendOp::add,
    .alpha_blend_op = BlendOp::add,
    .src_colour_factor = BlendFactor::one,
    .dst_colour_factor = BlendFactor::one,
    .src_alpha_factor = BlendFactor::one,
    .dst_alpha_factor = BlendFactor::one,
};

} // namespace Mg::gfx::blend_mode_constants
