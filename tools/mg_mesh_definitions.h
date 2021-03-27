//-----------------------------------------------------------------------------
// Mesh data structure definitions.
//-----------------------------------------------------------------------------

#pragma once

#include "mg_file_data_range.h"

#include <mg/utils/mg_vector_normalised.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace Mg {

constexpr size_t k_mesh_format_version = 2;
constexpr uint32_t k_four_cc = 0x444D474Du;

using JointId = uint8_t;
constexpr JointId joint_id_none = JointId(-1);

constexpr size_t k_max_num_children_per_joint = 8;
using JointChildren = std::array<JointId, k_max_num_children_per_joint>;

constexpr size_t k_num_joint_verts_per_vertex = 4;
using JointIds = std::array<JointId, k_num_joint_verts_per_vertex>;
using JointWeights = std::array<uint16_t, k_num_joint_verts_per_vertex>;

struct Vertex {
    glm::vec3 position = glm::vec3(0.0f);

    glm::vec2 uv0 = glm::vec2(0.0f);
    vec2_normalised uv1;

    vec3_normalised normal;
    vec3_normalised tangent;
    vec3_normalised bitangent;

    JointIds joint_id = {};
    JointWeights joint_weights = {}; // TODO store only 3, last is 1 - sum(joint_weights[0..2])
};

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

struct MeshHeader {
    uint32_t four_cc;
    uint32_t version;
    glm::vec3 centre;
    float radius;
    glm::vec3 abb_min;
    glm::vec3 abb_max;
    FileDataRange vertices;
    FileDataRange indices;
    FileDataRange submeshes;
    FileDataRange joints;
    FileDataRange strings;
};

using VertexIndex = uint16_t;

} // namespace Mg
