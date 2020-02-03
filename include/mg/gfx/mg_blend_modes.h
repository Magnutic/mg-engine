//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file renderer/mg_blend_modes.h
 * Blend mode types and predefined constants.
 */

#pragma once

namespace Mg::gfx {

/** Types of functions to use in blending */
enum class BlendFunc {
    ADD = 0x8006,
    SUBTRACT = 0x800A,
    REVERSE_SUBTRACT = 0x800B,
    MIN = 0x8007,
    MAX = 0x8008
};

/** Parameters for blend functions */
enum class BlendParam {
    ZERO = 0x0000,
    ONE = 0x0001,
    SRC_COLOUR = 0x0300,
    ONE_MINUS_SRC_COLOUR = 0x0301,
    SRC_ALPHA = 0x0302,
    ONE_MINUS_SRC_ALPHA = 0x0303,
    DST_ALPHA = 0x0304,
    ONE_MINUS_DST_ALPHA = 0x0305,
    DST_COLOUR = 0x0306,
    ONE_MINUS_DST_COLOUR = 0x0307
};

/** Describes a blend mode
 * For information on how to use blend mode, refer to e.g.:
 * https://www.opengl.org/wiki/Blending
 */
struct BlendMode {
    BlendFunc colour;
    BlendFunc alpha;
    BlendParam src_colour;
    BlendParam dst_colour;
    BlendParam src_alpha;
    BlendParam dst_alpha;
};

// TODO: make sure these blend modes are actually correct

/** Default BlendMode */
constexpr BlendMode c_blend_mode_default{ BlendFunc::ADD,   BlendFunc::ADD,  BlendParam::ONE,
                                          BlendParam::ZERO, BlendParam::ONE, BlendParam::ZERO };

/** Alpha BlendMode */
constexpr BlendMode c_blend_mode_alpha{ BlendFunc::ADD,        BlendFunc::ADD,
                                        BlendParam::SRC_ALPHA, BlendParam::ONE_MINUS_SRC_ALPHA,
                                        BlendParam::ONE,       BlendParam::ONE };

/** Premultiplied alpha BlendMode */
constexpr BlendMode c_blend_mode_alpha_premultiplied{
    BlendFunc::ADD,  BlendFunc::ADD, BlendParam::ONE, BlendParam::ONE_MINUS_SRC_ALPHA,
    BlendParam::ONE, BlendParam::ONE
};

/** Additive BlendMode */
constexpr BlendMode c_blend_mode_add{ BlendFunc::ADD,  BlendFunc::ADD,  BlendParam::SRC_ALPHA,
                                      BlendParam::ONE, BlendParam::ONE, BlendParam::ONE };

/** Premultiplied additive BlendMode */
constexpr BlendMode c_blend_mode_add_premultiplied{ BlendFunc::ADD,  BlendFunc::ADD,
                                                    BlendParam::ONE, BlendParam::ONE,
                                                    BlendParam::ONE, BlendParam::ONE };

} // namespace Mg::gfx
