//**************************************************************************************************
// Mg Engine
//-------------------------------------------------------------------------------
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

/** @file mg_vertex_attribute.h
 * Vertex attributes describe how vertex data is to be interpreted.
 */

#pragma once

#include <cstdint>

namespace Mg::gfx {

/** Vertex attributes describe how vertex data is to be interpreted. */
struct VertexAttribute {
    enum class Type : uint32_t {
        BYTE   = 0x1400,
        UBYTE  = 0x1401,
        SHORT  = 0x1402,
        USHORT = 0x1403,
        INT    = 0x1404,
        UINT   = 0x1405,
        FLOAT  = 0x1406,
        DOUBLE = 0x140A,
        HALF   = 0x140B,
        FIXED  = 0x140C
    };

    uint32_t num;        // Number of elements for attribute (e.g. 3 for a vec3)
    uint32_t size;       // Size in bytes for attribute
    Type     type;       // Type of attribute, OpenGL type enum values
    bool     normalised; // Whether integral data is to be interpreted as normalised (i.e. int range
                         // converted to [-1.0, 1.0])
};

} // namespace Mg::gfx
