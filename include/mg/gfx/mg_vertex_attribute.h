//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_vertex.h
 * Vertex attribute layout information.
 */

#pragma once

#include <array>
#include <cstdint>

namespace Mg::gfx {

/** Data type of vertex attribute elements. */
enum class VertexAttributeType : uint32_t {
    i8 = 0x1400,
    u8 = 0x1401,
    i16 = 0x1402,
    u16 = 0x1403,
    i32 = 0x1404,
    u32 = 0x1405,
    f32 = 0x1406,
    f64 = 0x140A
};

/** Interpretation of the value of vertex attributes integral type. */
enum class IntValueMeaning {
    /** Interpret value as a regular integer. */
    RegularInt,

    /** Intepret integral value as float by normalising the integral type's range to the range
     * [0.0f, 1.0f], if unsigned; or [-1.0f, 1.0f], if signed.
     */
    Normalise
};

/** Vertex attributes describe how vertex data is to be interpreted. */
struct VertexAttribute {
    // Number of elements for attribute (e.g. 3 for a vec3)
    uint32_t num_elements = 0;

    // Size in bytes for attribute
    uint32_t size = 0;

    // Type of attribute, OpenGL type enum values
    VertexAttributeType type = {};

    // Meaning of int values if type is an integral type.
    IntValueMeaning int_value_meaning = IntValueMeaning::RegularInt;
};

} // namespace Mg::gfx
