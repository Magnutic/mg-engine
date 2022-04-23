//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_joint.h
 * Joint, part of a skeleton, for animated meshes.
 */

#pragma once

#include <mg/core/mg_identifier.h>
#include <mg/gfx/mg_vertex_attribute.h>

#include <glm/mat4x4.hpp>

#include <array>
#include <cstdint>

namespace Mg::gfx::Mesh {

/** Identifier for a joint (used for animated meshes). */
using JointId = uint8_t;

/** Sentinel value reserved to mean no joint. */
constexpr JointId joint_id_none = JointId(-1);

/** Joints are a tree hierarchy. Each joint has a maximum number of children, defined here. */
constexpr size_t max_num_children_per_joint = 8;

/** Identifiers for each of a joint's children (or joint_id_none). */
using JointChildren = std::array<JointId, max_num_children_per_joint>;

constexpr size_t num_influences_per_vertex = 4;
using JointIds = std::array<JointId, num_influences_per_vertex>;
using JointWeights = std::array<uint16_t, num_influences_per_vertex>;

// Joint influences (for skinned/animated meshes).
// Describes which (up to) four joints influence a vertex, and by how much.
struct Influences {
    JointIds ids = { 0, 0, 0, 0 };
    JointWeights weights = { 0, 0, 0, 0 };
};

/** Attribute array corresponding to Influences. Describes the data layout of a vertex' joint
 * influences.
 */
constexpr std::array<VertexAttribute, 2> influences_attributes = {
    VertexAttribute{ 4, sizeof(Influences::ids), VertexAttributeType::u8 },

    VertexAttribute{ 4,
                     sizeof(Influences::weights),
                     VertexAttributeType::u16,
                     IntValueMeaning::Normalize }
};

struct Joint {
    glm::mat4 inverse_bind_matrix = glm::mat4(1.0f);
    JointChildren children = {};
    Identifier name;
};

} // namespace Mg::gfx::Mesh
