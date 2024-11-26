//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_block_scene.h
 * A scene consisting of a grid of blocks.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/core/mg_rotation.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"

#include <cfloat>
#include <map>
#include <vector>

namespace Mg {

namespace input {
class IInputSource;
}

namespace gfx {
class Camera;
class IRenderTarget;
class UIRenderer;
} // namespace gfx

class FontResource;

struct Block {
    float z_min = 0.0f;
    float z_max = 0.0f;
};

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

static constexpr size_t k_cluster_size = 32;

using BlockLevel = std::array<Block, k_cluster_size * k_cluster_size>;

inline size_t index_for_block(int x, int y)
{
    MG_ASSERT(x >= 0 && y >= 0);
    return size_t(y) * k_cluster_size + size_t(x);
}

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

struct BlockId {
    ClusterCoords cluster_coords{ 0, 0 };
    size_t level = 0;
    size_t block_index = 0;
};

class BlockScene {
public:
    explicit BlockScene(float block_size = 1.0f) : m_block_size{ block_size } {}

    void get_blocks_at(const int x, const int y, std::vector<Block>& blocks_out) const
    {
        const Cluster* cluster = cluster_covering(x, y);
        if (cluster) {
            const auto x_in_cluster = positive_modulo(x, int(k_cluster_size));
            const auto y_in_cluster = positive_modulo(y, int(k_cluster_size));

            for (auto& level : cluster->block_levels) {
                const auto& block = level[index_for_block(x_in_cluster, y_in_cluster)];
                if (valid(block)) {
                    blocks_out.push_back(block);
                }
            }
        }
    }

    bool try_insert(const int x, const int y, const Block to_insert)
    {
        const auto x_in_cluster = positive_modulo(x, int(k_cluster_size));
        const auto y_in_cluster = positive_modulo(y, int(k_cluster_size));
        const size_t block_index = index_for_block(x_in_cluster, y_in_cluster);
        auto& cluster = m_clusters[cluster_coords_for(x, y)];
        return try_insert_impl(cluster, block_index, to_insert);
    }

    [[nodiscard]] BlockSceneMesh make_mesh() const;

    float block_size() const { return m_block_size; }

    BlockId first_block_intersecting(const glm::vec3 start, const Rotation rotation)
    {
        MG_ASSERT(false);
        return {};
    }

private:
    ClusterCoords cluster_coords_for(int x, int y) const
    {
        return { (x < 0 ? x - (int(k_cluster_size - 1u)) : x) / int(k_cluster_size),
                 (y < 0 ? y - (int(k_cluster_size - 1u)) : y) / int(k_cluster_size) };
    }

    Cluster* cluster_covering(int x, int y)
    {
        return find_in_map(m_clusters, cluster_coords_for(x, y));
    }
    const Cluster* cluster_covering(int x, int y) const
    {
        return find_in_map(m_clusters, cluster_coords_for(x, y));
    }

    bool try_insert_impl(Cluster& cluster, const size_t block_index, const Block to_insert)
    {
        if (cluster.block_levels.empty()) {
            auto& level = cluster.block_levels.emplace_back();
            level[block_index] = to_insert;
            return true;
        }

        for (auto& block_level : cluster.block_levels) {
            Block& current_block = block_level[block_index];
            const bool prevents_insertion = valid(current_block) &&
                                            current_block.z_min < to_insert.z_max &&
                                            current_block.z_max > to_insert.z_min;
            if (prevents_insertion) {
                return false;
            }

            const bool can_insert = !valid(current_block) || current_block.z_min >= to_insert.z_max;
            if (can_insert) {
                // Place the new block on this level, overwriting the block currently in this level.
                const Block old_block = std::exchange(current_block, to_insert);

                // Re-insert the old block above the current one.
                if (valid(old_block)) {
                    try_insert_impl(cluster, block_index, old_block);
                }
                return true;
            }
        }

        // If we got here, we need to add a new level.
        cluster.block_levels.emplace_back()[block_index] = to_insert;
        return true;
    }

    std::map<ClusterCoords, Cluster> m_clusters;
    float m_block_size;
};

class BlockSceneEditor {
public:
    explicit BlockSceneEditor(BlockScene& scene,
                              input::IInputSource& input_source,
                              ResourceHandle<FontResource> font_resource);

    void render(const gfx::Camera& cam, const gfx::IRenderTarget& render_target);

    void render_ui(const gfx::IRenderTarget& render_target, gfx::UIRenderer& ui_renderer);

    /** Run editor step. Returns whether any changes were made to the scene. */
    bool update(const glm::vec3& view_position, const glm::vec3& view_angle);

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

} // namespace Mg
