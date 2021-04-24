//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_data.h
 * Definitions representing mesh data.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_joint.h"
#include "mg/gfx/mg_vertex_attribute.h"
#include "mg/mg_bounding_volumes.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_vector_normalised.h"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <limits>

/** Definitions related to meshes. */
namespace Mg::gfx::Mesh {

//--------------------------------------------------------------------------------------------------
// Fundamental mesh vertex definitions
//--------------------------------------------------------------------------------------------------

/** Unsigned integer type used to represent mesh-vertex indices. */
using Index = uint16_t;

/** Limitation: meshes cannot have more vertices than is representable by Index. */
constexpr auto max_vertices_per_mesh = std::numeric_limits<Index>::max();

struct Vertex {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec2 tex_coord = glm::vec2(0.0f);

    uint32_t padding; // TODO temp for compatibility.

    vec3_normalised normal;
    vec3_normalised tangent;
    vec3_normalised bitangent;
};

/** Attribute array corresponding to Vertex. Describes the data layout of a vertex. */
constexpr std::array<VertexAttribute, 5> mesh_vertex_attributes = {
    VertexAttribute{ 3, sizeof(Vertex::position), VertexAttributeType::f32 },

    VertexAttribute{ 2,
                     sizeof(Vertex::tex_coord) + sizeof(Vertex::padding),
                     VertexAttributeType::f32 },

    VertexAttribute{ 3,
                     sizeof(Vertex::normal),
                     VertexAttributeType::i16,
                     IntValueMeaning::Normalise },

    VertexAttribute{ 3,
                     sizeof(Vertex::tangent),
                     VertexAttributeType::i16,
                     IntValueMeaning::Normalise },

    VertexAttribute{ 3,
                     sizeof(Vertex::bitangent),
                     VertexAttributeType::i16,
                     IntValueMeaning::Normalise }
};

//--------------------------------------------------------------------------------------------------
// Mesh structure definitions
//--------------------------------------------------------------------------------------------------

/** Range of indices belonging to a submesh. */
struct SubmeshRange {
    uint32_t begin;
    uint32_t amount;
};

/** A submesh is a subset of the vertices of a mesh that is rendered separately. Each submesh may be
 * rendered with a different material.
 */
struct Submesh {
    SubmeshRange index_range;
    Identifier name{ "" };
};

/** Non-owning view over the data required to define a mesh. */
struct MeshDataView {
    /** The vertices making up the mesh. */
    span<const Vertex> vertices;

    /** Indices into `vertices` buffer, defining triangle list. */
    span<const Index> indices;

    /** Submeshes as defined by a range of `indices`. */
    span<const Submesh> submeshes;

    /** Per-vertex influences of skeleton joints for animation. Should either be empty (for
     * non-animated meshes), or the same size as `MeshDataView::vertices`.
     */
    span<const Influences> influences;

    /** Optionally store bounding sphere here; otherwise, it will be calculated when needed. */
    Opt<BoundingSphere> bounding_sphere;
};

} // namespace Mg::gfx::Mesh
