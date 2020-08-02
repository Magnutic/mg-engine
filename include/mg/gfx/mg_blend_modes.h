//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file renderer/mg_blend_modes.h
 * Blend mode types and predefined constants.
 */

#pragma once

/** Pre-defined blend modes. */
namespace Mg::gfx {

enum class BlendFactor {
    zero = 0,
    one = 1,
    src_colour = 2,
    one_minus_src_colour = 3,
    src_alpha = 4,
    one_minus_src_alpha = 5,
    dst_alpha = 6,
    one_minus_dst_alpha = 7,
    dst_colour = 8,
    one_minus_dst_colour = 9
};

enum class BlendOp { add = 0, subtract = 1, reverse_subtract = 2, min = 3, max = 4 };

struct BlendMode {
    BlendOp colour_blend_op : 3;
    BlendOp alpha_blend_op : 3;
    BlendFactor src_colour_factor : 6;
    BlendFactor dst_colour_factor : 6;
    BlendFactor src_alpha_factor : 6;
    BlendFactor dst_alpha_factor : 6;
};

inline bool operator==(BlendMode l, BlendMode r)
{
    return l.colour_blend_op == r.colour_blend_op && l.alpha_blend_op == r.alpha_blend_op &&
           l.src_alpha_factor == r.src_alpha_factor && l.dst_colour_factor == r.dst_colour_factor &&
           l.src_alpha_factor == r.src_alpha_factor && l.dst_alpha_factor == r.dst_alpha_factor;
}

inline bool operator!=(BlendMode l, BlendMode r)
{
    return !(l == r);
}

} // namespace Mg::gfx

namespace Mg::gfx::blend_mode_constants {

/** Default BlendMode */
constexpr BlendMode bm_default{ BlendOp::add,      BlendOp::add,     BlendFactor::one,
                                BlendFactor::zero, BlendFactor::one, BlendFactor::zero };

/** Alpha BlendMode */
constexpr BlendMode bm_alpha{ BlendOp::add,           BlendOp::add,
                              BlendFactor::src_alpha, BlendFactor::one_minus_src_alpha,
                              BlendFactor::one,       BlendFactor::one };

/** Premultiplied alpha BlendMode */
constexpr BlendMode bm_alpha_premultiplied{ BlendOp::add,     BlendOp::add,
                                            BlendFactor::one, BlendFactor::one_minus_src_alpha,
                                            BlendFactor::one, BlendFactor::one };

/** Additive BlendMode */
constexpr BlendMode bm_add{ BlendOp::add,     BlendOp::add,     BlendFactor::src_alpha,
                            BlendFactor::one, BlendFactor::one, BlendFactor::one };

/** Premultiplied additive BlendMode */
constexpr BlendMode bm_add_premultiplied{ BlendOp::add,     BlendOp::add,     BlendFactor::one,
                                          BlendFactor::one, BlendFactor::one, BlendFactor::one };

} // namespace Mg::gfx::blend_mode_constants
