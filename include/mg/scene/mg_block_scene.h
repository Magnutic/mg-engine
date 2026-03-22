//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_block_scene.h
 * A scene consisting of a grid of blocks.
 */

#pragma once

#include "mg/core/gfx/mg_mesh_data.h"

#include <cfloat>
#include <map>
#include <type_traits>
#include <vector>

namespace Mg {

enum class BlockFace { west, north, east, south, bottom, top };
inline constexpr size_t k_block_scene_cluster_size = 32;
inline constexpr size_t k_block_scene_max_num_textures = UINT8_MAX;

struct BlockTextures {
    uint8_t& operator[](BlockFace face) { return textures[static_cast<size_t>(face)]; }
    const uint8_t& operator[](BlockFace face) const { return textures[static_cast<size_t>(face)]; }

    std::array<uint8_t, 6> textures;

    friend bool operator==(const BlockTextures&, const BlockTextures&) = default;
    friend bool operator!=(const BlockTextures&, const BlockTextures&) = default;
};

struct Block {
    float z_min;
    float z_max;
    BlockTextures textures{};

    friend bool operator==(const Block&, const Block&) = default;
    friend bool operator!=(const Block&, const Block&) = default;
};

static_assert(std::is_trivially_copyable_v<Block>);

inline bool valid(const Block& block)
{
    return block.z_min < block.z_max;
}

class BlockSceneMesh {
public:
    gfx::mesh_data::MeshDataView view() const
    {
        return { .vertices = m_vertices, .indices = m_indices, .submeshes = m_submeshes };
    }

private:
    friend class BlockScene;

    std::vector<gfx::mesh_data::Vertex> m_vertices;
    std::vector<gfx::mesh_data::Index> m_indices;
    std::vector<gfx::mesh_data::Submesh> m_submeshes;
};

using BlockLevel = std::array<Block, k_block_scene_cluster_size * k_block_scene_cluster_size>;

struct Cluster {
    std::vector<BlockLevel> block_levels;
};

struct ClusterCoords {
    int x;
    int y;
};

inline bool operator<(const ClusterCoords& lhs, const ClusterCoords& rhs)
{
    return lhs.y < rhs.y || (lhs.y == rhs.y && lhs.x < rhs.x);
}

class BlockScene {
public:
    using Clusters = std::map<ClusterCoords, Cluster>;

    explicit BlockScene(float block_size = 1.0f, Clusters&& clusters = {})
        : m_clusters{ std::move(clusters) }, m_block_size{ block_size }
    {}

    void get_blocks_at(int x, int y, std::vector<Block>& blocks_out) const;

    bool try_insert(int x, int y, Block to_insert);

    bool try_delete(int x, int y, size_t level);

    Opt<BlockTextures&> block_textures_at(int x, int y, size_t level);

    [[nodiscard]] BlockSceneMesh make_mesh(size_t num_textures) const;

    float block_size() const { return m_block_size; }

    const Clusters& internal_representation() const { return m_clusters; }

private:
    ClusterCoords cluster_coords_for(int x, int y) const;

    Cluster* cluster_covering(int x, int y);
    const Cluster* cluster_covering(int x, int y) const;

    bool try_insert_impl(Cluster& cluster, size_t block_index, Block to_insert);

    Clusters m_clusters;
    float m_block_size;
};


} // namespace Mg
