//**************************************************************************************************
// Mg Engine
//-------------------------------------------------------------------------------
// Copyright (c) 2020 Magnus Bergsten
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

/** @file mg_mesh_data.h
 * Definitions representing mesh data.
 */

#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_vector_normalised.h"

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

// Unsigned integer type used to represent mesh-vertex indices.
using uint_vertex_index                = uint16_t;
constexpr auto k_max_vertices_per_mesh = std::numeric_limits<uint_vertex_index>::max();

// I expect the struct elements to be naturally aligned and not padded, but deactivate padding just
// to be sure.
#pragma pack(push, 1)

/** General vertex type. */
struct Vertex {
    glm::vec3 position = glm::vec3(0.0f);

    // Texture coordinates
    glm::vec2 uv0 = glm::vec2(0.0f);

    // Secondary texture coordinates, e.g. for lightmaps. Limited to [0.0,  1.0] for space reasons.
    vec2_normalised uv1;

    // Normal, tangent, and bitangent vectors. Used to calculate e.g. how light affects the surface.
    vec3_normalised normal;
    vec3_normalised tangent;
    vec3_normalised bitangent;

    // Joint bindings (for skinned/animated meshes). Describes which (up to) four joints affect this
    // vertex, and how much.
    std::array<uint8_t, 4> joint_id{ { 0, 0, 0, 0 } };
    vec4_normalised        joint_weights{ 0.0f, 0.0f, 0.0f, 0.0f };
};

#pragma pack(pop)

/** Attribute array corresponding to Vertex. Describes the data layout of a vertex. */
constexpr std::array<gfx::VertexAttribute, 8> g_attrib_array{
    { { 3, sizeof(Vertex::position), gfx::VertexAttribute::Type::FLOAT, false },

      { 2, sizeof(Vertex::uv0), gfx::VertexAttribute::Type::FLOAT, false },
      { 2, sizeof(Vertex::uv1), gfx::VertexAttribute::Type::SHORT, true },

      { 3, sizeof(Vertex::normal), gfx::VertexAttribute::Type::SHORT, true },
      { 3, sizeof(Vertex::tangent), gfx::VertexAttribute::Type::SHORT, true },
      { 3, sizeof(Vertex::bitangent), gfx::VertexAttribute::Type::SHORT, true },

      { 4, sizeof(Vertex::joint_id), gfx::VertexAttribute::Type::UBYTE, false },
      { 4, sizeof(Vertex::joint_weights), gfx::VertexAttribute::Type::SHORT, true } }
};

/** A submesh is a subset of the vertices of a mesh that is rendered separately (each submesh may be
 * rendered with a different material).
 */
struct SubMesh {
    uint32_t   begin  = 0;
    uint32_t   amount = 0;
    Identifier name{ "" };
};

/** A non-owning view over the data required to define a mesh. */
struct MeshDataView {
    span<const Vertex>            vertices;
    span<const uint_vertex_index> indices;
    span<const SubMesh>           sub_meshes;

    struct BoundingInfo {
        glm::vec3 centre = {};
        float     radius = 0.0f;
    };
    Opt<BoundingInfo> bounding_info;
};

} // namespace Mg
