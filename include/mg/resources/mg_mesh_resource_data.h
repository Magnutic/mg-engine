//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_resource_data.h
 * Data definitions for mesh resource file type.
 */

#pragma once

#include "mg/gfx/mg_animation.h"
#include "mg/gfx/mg_joint.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/mg_file_data_range.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstdint>

/** Data structure definitions and constants for the Mg mesh file format. */
namespace Mg::MeshResourceData {

constexpr uint32_t fourcc = 0x444D474Du; // MGMD
constexpr uint32_t version = 2;          // Current version of the file format.

using gfx::mesh_data::joint_id_none;
using gfx::mesh_data::max_num_children_per_joint;
using gfx::mesh_data::max_vertices_per_mesh;
using gfx::mesh_data::num_influences_per_vertex;

using gfx::mesh_data::Index;
using gfx::mesh_data::Influences;
using gfx::mesh_data::JointChildren;
using gfx::mesh_data::JointId;
using gfx::mesh_data::JointIds;
using gfx::mesh_data::JointWeights;
using gfx::mesh_data::SubmeshRange;
using gfx::mesh_data::Vertex;

using gfx::mesh_data::PositionKey;
using gfx::mesh_data::RotationKey;
using gfx::mesh_data::ScaleKey;

struct Header {
    uint32_t four_cc;
    uint32_t version;
    glm::vec3 centre;
    float radius;
    glm::vec3 abb_min;
    glm::vec3 abb_max;
    glm::mat4 skeleton_root_transform;
    FileDataRange vertices;
    FileDataRange indices;
    FileDataRange submeshes;
    FileDataRange joints;
    FileDataRange influences;
    FileDataRange animations;
    FileDataRange strings;
};

/** At the end of each mesh file there is a buffer of zero-terminated strings. This struct points
 * out a string within said buffer.
 */
struct StringRange {
    uint32_t begin = 0; // Index into the string buffer
    uint32_t length = 0;
};

struct Submesh {
    StringRange name;
    StringRange material;
    uint32_t begin = 0;
    uint32_t num_indices = 0;
};

struct Joint {
    StringRange name;
    glm::mat4 inverse_bind_matrix{ 0.0f };
    JointChildren children = {};
};

struct AnimationClip {
    StringRange name;
    FileDataRange channels;
    double duration = 0.0;
};

struct AnimationChannel {
    FileDataRange position_keys;
    FileDataRange rotation_keys;
    FileDataRange scale_keys;
};

} // namespace Mg::MeshResourceData
