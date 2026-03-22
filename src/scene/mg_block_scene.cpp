//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/scene/mg_block_scene.h"

#include <cmath>

#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"

namespace Mg {

namespace {

std::pair<int, int> coords_within_cluster(int x, int y)
{
    return { positive_modulo(x, int(k_block_scene_cluster_size)),
             positive_modulo(y, int(k_block_scene_cluster_size)) };
}

size_t index_for_block(int x, int y)
{
    MG_ASSERT(x >= 0 && y >= 0);
    return size_t(y) * k_block_scene_cluster_size + size_t(x);
}

template<std::invocable<float, float> F>
void for_each_gap(std::span<const Block> neighbours, const float z_min, const float z_max, F f)
{
    float next_gap_start = z_min;
    auto apply_if_nonempty = [&f](float z1, float z2) {
        if (z1 < z2) {
            f(z1, z2);
        }
    };

    for (const Block& block : neighbours) {
        if (block.z_min <= next_gap_start && block.z_max >= next_gap_start) {
            next_gap_start = block.z_max;
            continue;
        }

        if (block.z_min > next_gap_start && block.z_min < z_max) {
            apply_if_nonempty(next_gap_start, block.z_min);
            next_gap_start = block.z_max;
        }

        if (next_gap_start >= z_max) {
            return;
        }
    }

    apply_if_nonempty(next_gap_start, z_max);
}

} // namespace

BlockSceneMesh BlockScene::make_mesh(const size_t num_textures) const
{
    BlockSceneMesh result;

    std::vector<Block> blocks;

    auto add_block_to_mesh = [&](const Block block, int x, int y) {
        if (block.z_min >= block.z_max) {
            return;
        }

        auto start_index = as<uint32_t>(result.m_vertices.size());

        // TODO check V component of UV coords. Flipped? Textures flipped in memory?

        auto add_wall =
            [&](float x1, float y1, float x2, float y2, float z1, float z2, BlockFace face) {
                const auto tex_coord_z = float(block.textures[face] % num_textures);
                const float tex_coord_bottom = std::fmod(z1, m_block_size);
                const float tex_coord_top = tex_coord_bottom + (z2 - z1) / m_block_size;

                {
                    auto& v = result.m_vertices.emplace_back();
                    v.position = glm::vec3(x1, y1, z1);
                    v.tex_coord =
                        glm::vec3((x1 == x2 ? 1.0f : 0.0f), tex_coord_bottom, tex_coord_z);
                }

                {
                    auto& v = result.m_vertices.emplace_back();
                    v.position = glm::vec3(x2, y1, (x1 == x2 ? z2 : z1));
                    v.tex_coord =
                        glm::vec3(1.0f, (x1 == x2 ? tex_coord_top : tex_coord_bottom), tex_coord_z);
                }

                {
                    auto& v = result.m_vertices.emplace_back();
                    v.position = glm::vec3(x1, y2, (x1 == x2 ? z1 : z2));
                    v.tex_coord =
                        glm::vec3(0.0f, (x1 == x2 ? tex_coord_bottom : tex_coord_top), tex_coord_z);
                }

                {
                    auto& v = result.m_vertices.emplace_back();
                    v.position = glm::vec3(x2, y2, z2);
                    v.tex_coord = glm::vec3((x1 == x2 ? 0.0f : 1.0f), tex_coord_top, tex_coord_z);
                }

                const bool flip_winding = face == BlockFace::east || face == BlockFace::north;
                const bool flip_normal = face == BlockFace::west || face == BlockFace::north;
                const bool flip_tangent = face == BlockFace::west || face == BlockFace::east;
                const bool flip_bitangent = face == BlockFace::west || face == BlockFace::south;

                const auto normal = glm::normalize(glm::cross(glm::vec3{ x2 - x1, y2 - y1, 0.0f },
                                                              glm::vec3{ 0.0f, 0.0f, 1.0f })) *
                                    (flip_normal ? -1.0f : 1.0f);
                const auto tangent = glm::normalize(glm::vec3{ x2 - x1, y2 - y1, 0.0f }) *
                                     (flip_tangent ? -1.0f : 1.0f);
                const auto bitangent = glm::cross(normal, tangent) *
                                       (flip_bitangent ? -1.0f : 1.0f);

                for (size_t i = result.m_vertices.size() - 4u; i < result.m_vertices.size(); ++i) {
                    auto& v = result.m_vertices[i];
                    v.normal = normal;
                    v.tangent = tangent;
                    v.bitangent = bitangent;
                }

                if (flip_winding) {
                    result.m_indices.push_back(start_index);
                    result.m_indices.push_back(start_index + 2);
                    result.m_indices.push_back(start_index + 1);
                    result.m_indices.push_back(start_index + 3);
                    result.m_indices.push_back(start_index + 1);
                    result.m_indices.push_back(start_index + 2);
                }
                else {
                    result.m_indices.push_back(start_index);
                    result.m_indices.push_back(start_index + 1);
                    result.m_indices.push_back(start_index + 2);
                    result.m_indices.push_back(start_index + 2);
                    result.m_indices.push_back(start_index + 1);
                    result.m_indices.push_back(start_index + 3);
                }

                start_index += 4;
            };

        const auto x1 = float(x) * m_block_size;
        const auto y1 = float(y) * m_block_size;
        const auto x2 = x1 + m_block_size;
        const auto y2 = y1 + m_block_size;

        blocks.clear();
        get_blocks_at(x, y, blocks);
        const bool skip_bottom_flat = std::ranges::find_if(blocks, [&block](const Block& b) {
                                          return b.z_max == block.z_min;
                                      }) != blocks.end();

        const bool skip_top_flat = std::ranges::find_if(blocks, [&block](const Block& b) {
                                       return b.z_min == block.z_max;
                                   }) != blocks.end();

        // Bottom
        if (!skip_bottom_flat) {
            const auto tex_coord_z = float(block.textures[BlockFace::bottom] % num_textures);
            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x1, y1, block.z_min);
                v.tex_coord = glm::vec3(0.0f, 0.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x2, y1, block.z_min);
                v.tex_coord = glm::vec3(1.0f, 0.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x1, y2, block.z_min);
                v.tex_coord = glm::vec3(0.0f, 1.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x2, y2, block.z_min);
                v.tex_coord = glm::vec3(1.0f, 1.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, -1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            result.m_indices.push_back(start_index);
            result.m_indices.push_back(start_index + 2);
            result.m_indices.push_back(start_index + 1);
            result.m_indices.push_back(start_index + 2);
            result.m_indices.push_back(start_index + 3);
            result.m_indices.push_back(start_index + 1);

            start_index += 4;
        }

        // Top
        if (!skip_top_flat) {
            const auto tex_coord_z = float(block.textures[BlockFace::top] % num_textures);
            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x1, y1, block.z_max);
                v.tex_coord = glm::vec3(0.0f, 0.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x2, y1, block.z_max);
                v.tex_coord = glm::vec3(1.0f, 0.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x1, y2, block.z_max);
                v.tex_coord = glm::vec3(0.0f, 1.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            {
                auto& v = result.m_vertices.emplace_back();
                v.position = glm::vec3(x2, y2, block.z_max);
                v.tex_coord = glm::vec3(1.0f, 1.0f, tex_coord_z);
                v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.bitangent = glm::vec3(0.0f, -1.0f, 0.0f);
            }

            result.m_indices.push_back(start_index);
            result.m_indices.push_back(start_index + 1);
            result.m_indices.push_back(start_index + 2);
            result.m_indices.push_back(start_index + 2);
            result.m_indices.push_back(start_index + 1);
            result.m_indices.push_back(start_index + 3);

            start_index += 4;
        }

        // -X
        blocks.clear();
        get_blocks_at(x - 1, y, blocks);
        for_each_gap(blocks, block.z_min, block.z_max, [&](float z1, float z2) {
            add_wall(x1, y1, x1, y2, z1, z2, BlockFace::west);
        });

        // +X
        blocks.clear();
        get_blocks_at(x + 1, y, blocks);
        for_each_gap(blocks, block.z_min, block.z_max, [&](float z1, float z2) {
            add_wall(x2, y1, x2, y2, z1, z2, BlockFace::east);
        });

        // -Y
        blocks.clear();
        get_blocks_at(x, y - 1, blocks);
        for_each_gap(blocks, block.z_min, block.z_max, [&](float z1, float z2) {
            add_wall(x1, y1, x2, y1, z1, z2, BlockFace::south);
        });

        // +Y
        blocks.clear();
        get_blocks_at(x, y + 1, blocks);
        for_each_gap(blocks, block.z_min, block.z_max, [&](float z1, float z2) {
            add_wall(x1, y2, x2, y2, z1, z2, BlockFace::north);
        });
    };

    for (const auto& [cluster_coord, cluster] : m_clusters) {
        const auto cluster_x_min = cluster_coord.x * int(k_block_scene_cluster_size);
        const auto cluster_y_min = cluster_coord.y * int(k_block_scene_cluster_size);

        for (const auto& level : cluster.block_levels) {
            for (int y = 0; y < int(k_block_scene_cluster_size); ++y) {
                for (int x = 0; x < int(k_block_scene_cluster_size); ++x) {
                    const Block block = level[index_for_block(x, y)];
                    add_block_to_mesh(block, cluster_x_min + x, cluster_y_min + y);
                }
            }
        }
    }

    result.m_submeshes.push_back(gfx::mesh_data::Submesh{
        .index_range{ 0u, as<uint32_t>(result.m_indices.size()) },
        .name = "BlockScene",
        .material_binding_id = "SceneMaterial0",
    });

    return result;
}

void BlockScene::get_blocks_at(const int x, const int y, std::vector<Block>& blocks_out) const
{
    const Cluster* cluster = cluster_covering(x, y);
    if (cluster) {
        const auto [x_in_cluster, y_in_cluster] = coords_within_cluster(x, y);

        for (auto& level : cluster->block_levels) {
            const auto& block = level[index_for_block(x_in_cluster, y_in_cluster)];
            if (!valid(block)) {
                break;
            }
            blocks_out.push_back(block);
        }
    }
}

bool BlockScene::try_insert(const int x, const int y, const Block to_insert)
{
    const auto [x_in_cluster, y_in_cluster] = coords_within_cluster(x, y);
    const size_t block_index = index_for_block(x_in_cluster, y_in_cluster);
    auto& cluster = m_clusters[cluster_coords_for(x, y)];
    return try_insert_impl(cluster, block_index, to_insert);
}

bool BlockScene::try_delete(int x, int y, size_t level)
{
    auto* cluster = cluster_covering(x, y);
    if (!cluster) {
        return false;
    }

    const auto [x_in_cluster, y_in_cluster] = coords_within_cluster(x, y);
    const size_t block_index = index_for_block(x_in_cluster, y_in_cluster);
    auto& block = cluster->block_levels.at(level).at(block_index);
    const auto success = valid(block);
    std::memset(&block, 0, sizeof(block));
    for (auto i = level; i + 1 < cluster->block_levels.size(); ++i) {
        cluster->block_levels[i].at(block_index) = cluster->block_levels[i + 1].at(block_index);
    }
    return success;
}

Opt<BlockTextures&> BlockScene::block_textures_at(int x, int y, size_t level)
{
    auto* cluster = cluster_covering(x, y);
    if (!cluster) {
        return nullopt;
    }

    const auto [x_in_cluster, y_in_cluster] = coords_within_cluster(x, y);
    const size_t block_index = index_for_block(x_in_cluster, y_in_cluster);
    auto& block = cluster->block_levels.at(level).at(block_index);
    return block.textures;
}

ClusterCoords BlockScene::cluster_coords_for(int x, int y) const
{
    return {
        (x < 0 ? x - (int(k_block_scene_cluster_size - 1u)) : x) / int(k_block_scene_cluster_size),
        (y < 0 ? y - (int(k_block_scene_cluster_size - 1u)) : y) / int(k_block_scene_cluster_size)
    };
}

Cluster* BlockScene::cluster_covering(int x, int y)
{
    return find_in_map(m_clusters, cluster_coords_for(x, y));
}

const Cluster* BlockScene::cluster_covering(int x, int y) const
{
    return find_in_map(m_clusters, cluster_coords_for(x, y));
}

bool BlockScene::try_insert_impl(Cluster& cluster, const size_t block_index, const Block to_insert)
{
    auto add_block_level = [&cluster]() -> BlockLevel& {
        auto& level = cluster.block_levels.emplace_back();
        std::memset(level.data(), 0, level.size() * sizeof(level[0]));
        return level;
    };

    if (cluster.block_levels.empty()) {
        auto& level = add_block_level();
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
    add_block_level()[block_index] = to_insert;
    return true;
}

} // namespace Mg
