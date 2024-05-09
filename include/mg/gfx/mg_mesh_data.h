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
#include "mg/gfx/mg_animation.h"
#include "mg/gfx/mg_joint.h"
#include "mg/gfx/mg_vertex_attribute.h"
#include "mg/mg_bounding_volumes.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_vector_normalized.h"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <limits>

/** Definitions related to meshes. */
namespace Mg::gfx::mesh_data {

//--------------------------------------------------------------------------------------------------
// Fundamental mesh vertex definitions
//--------------------------------------------------------------------------------------------------

/** Unsigned integer type used to represent mesh-vertex indices. */
using Index = uint32_t;

/** Limitation: meshes cannot have more vertices than is representable by Index. */
constexpr auto max_vertices_per_mesh = std::numeric_limits<Index>::max();

struct Vertex {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec2 tex_coord = glm::vec2(0.0f);

    uint32_t padding = 0; // TODO temp for compatibility.

    vec3_normalized normal;
    vec3_normalized tangent;
    vec3_normalized bitangent;
};

/** Attribute array corresponding to Vertex. Describes the data layout of a vertex. */
constexpr std::array<VertexAttribute, 5> vertex_attributes = {
    VertexAttribute{ .identifier = "position",
                     .binding_location = 0,
                     .num_elements = 3,
                     .size = sizeof(Vertex::position),
                     .type = VertexAttributeType::f32 },

    VertexAttribute{ .identifier = "tex_coord",
                     .binding_location = 1,
                     .num_elements = 2,
                     .size = sizeof(Vertex::tex_coord) + sizeof(Vertex::padding),
                     .type = VertexAttributeType::f32 },

    VertexAttribute{ .identifier = "normal",
                     .binding_location = 2,
                     .num_elements = 3,
                     .size = sizeof(Vertex::normal),
                     .type = VertexAttributeType::i16,
                     .int_value_meaning = IntValueMeaning::Normalize },

    VertexAttribute{ .identifier = "tangent",
                     .binding_location = 3,
                     .num_elements = 3,
                     .size = sizeof(Vertex::tangent),
                     .type = VertexAttributeType::i16,
                     .int_value_meaning = IntValueMeaning::Normalize },

    VertexAttribute{ .identifier = "bitangent",
                     .binding_location = 4,
                     .num_elements = 3,
                     .size = sizeof(Vertex::bitangent),
                     .type = VertexAttributeType::i16,
                     .int_value_meaning = IntValueMeaning::Normalize }
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
    SubmeshRange index_range = {};
    Identifier name{ "" };

    // Conversion from simple range, for compatibility.
    explicit Submesh(const SubmeshRange& range) : index_range(range) {}

    Submesh() = default;

    Submesh(const SubmeshRange& range, const Identifier name_) : index_range(range), name(name_) {}
};

/** Non-owning view over the data required to define animations in a mesh. */
struct AnimationDataView {
    /** Per-vertex influences of skeleton joints for animation. Should either be empty (for
     * non-animated meshes), or the same size as `MeshDataView::vertices`.
     */
    std::span<const Influences> influences;
    std::span<const Joint> joints;
    std::span<const AnimationClip> animation_clips;

    glm::mat4 skeleton_root_transform;
};

/** Non-owning view over the data required to define a mesh. */
struct MeshDataView {
    /** The vertices making up the mesh. */
    std::span<const Vertex> vertices;

    /** Indices into `vertices` buffer, defining triangle list. */
    std::span<const Index> indices;

    /** Submeshes as defined by a range of `indices`. */
    std::span<const Submesh> submeshes;

    /** Optionally store animation date here; if not present, the mesh has no animations. */
    Opt<AnimationDataView> animation_data;

    /** Optionally store bounding sphere here; otherwise, it will be calculated when needed. */
    Opt<BoundingSphere> bounding_sphere;

    /** Optionally store bounding box; otherwise, it will be calculated when needed. */
    Opt<AxisAlignedBoundingBox> aabb;
};

/** Strongly typed size type for vertex buffers, specified in number of bytes.
 * Can be cast to and from size_t using static_cast.
 * Can also be constructed directly, for example: `VertexBufferSize{ 512 }`
 */
enum class VertexBufferSize : size_t;

/** Strongly typed size type for vertex index buffers, specified in number of bytes.
 * Can be cast to and from size_t using static_cast.
 * Can also be constructed directly, for example: `IndexBufferSize{ 512 }`
 */
enum class IndexBufferSize : size_t;

/** Strongly typed size type for vertex influences buffers, specified in number of bytes.
 * Can be cast to and from size_t using static_cast.
 * Can also be constructed directly, for example: `InfluencesBufferSize{ 512 }`
 */
enum class InfluencesBufferSize : size_t;

} // namespace Mg::gfx::mesh_data
