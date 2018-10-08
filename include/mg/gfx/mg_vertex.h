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

/** @file mg_vertex.h
 * General 3D-mesh vertex type.
 */

#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <mg/gfx/mg_vertex_attribute.h>
#include <mg/utils/mg_vector_normalised.h>

namespace Mg {

// Unsigned integer type used to represent mesh-vertex indices.
using uint_vertex_index                = uint16_t;
constexpr auto k_max_vertices_per_mesh = std::numeric_limits<uint_vertex_index>::max();

// I expect the struct elements to be naturally aligned and not padded, but deactivate padding just
// to be sure.
#pragma pack(push, 1)

/** General Vertex type. */
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

} // namespace Mg
