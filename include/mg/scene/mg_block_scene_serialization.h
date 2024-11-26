//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_block_scene_serialization.h
 * .
 */

#pragma once

#include "mg/core/mg_log.h"
#include "mg/scene/mg_block_scene.h"
#include "mg/utils/mg_file_io.h"
#include "mg/utils/mg_fourcc.h"

namespace Mg {

static constexpr uint32_t k_block_scene_filetype_identifier = make_fourcc("MGBS");
static constexpr uint32_t k_block_scene_filetype_version = 1;

struct BlockSceneHeader {
    uint32_t filetype_identifier_4cc;
    uint32_t filetype_version;
    uint32_t cluster_size;
    uint32_t num_clusters;
    float block_size;
    uint32_t cluster_data_offset;
};

struct ClusterHeader {
    int x;
    int y;
    uint32_t num_levels;
    uint32_t level_data_offset;
};

inline std::pair<bool, std::string>
write_block_scene(const BlockScene& scene, std::string_view file_path, bool overwrite)
{
    auto [writer, error] = io::make_output_filestream(file_path, overwrite, io::Mode::binary);

    if (!writer.has_value()) {
        auto msg = fmt::format("Failed to open file '{}' for writing: {}", file_path, error);
        log.error(msg);
        return { false, std::move(msg) };
    }

    const BlockScene::Clusters& clusters = scene.internal_representation();

    {
        BlockSceneHeader file_header{
            .filetype_identifier_4cc = k_block_scene_filetype_identifier,
            .filetype_version = k_block_scene_filetype_version,
            .cluster_size = k_block_scene_cluster_size,
            .num_clusters = narrow<uint32_t>(clusters.size()),
            .block_size = scene.block_size(),
            .cluster_data_offset = 0,
        };

        io::write_binary(writer.value(), file_header);
    }

    for (const auto& cluster : clusters) {
        ClusterHeader cluster_header{
            .x = cluster.first.x,
            .y = cluster.first.y,
            .num_levels = narrow<uint32_t>(cluster.second.block_levels.size()),
            .level_data_offset = 0,
        };

        io::write_binary(writer.value(), cluster_header);

        for (const auto& level : cluster.second.block_levels) {
            io::write_binary_array<Mg::Block>(writer.value(), level);
        }
    }

    return { true, "" };
}

inline Opt<BlockScene> read_block_scene(std::string_view file_path)
{
    auto [reader, error] = io::make_input_filestream(file_path, io::Mode::binary);
    if (!reader) {
        log.error("Failed to open file '{}' for reading: {}", file_path, error);
        return nullopt;
    }

    BlockSceneHeader file_header{};
    io::read_binary(reader.value(), file_header);

    if (file_header.filetype_identifier_4cc != k_block_scene_filetype_identifier) {
        log.error("File '{}' is not an Mg Engine block scene", file_path);
        return nullopt;
    }

    if (file_header.cluster_size != k_block_scene_cluster_size) {
        log.error("File '{}' has incompatible cluster size");
        return nullopt;
    }

    if (file_header.filetype_version > k_block_scene_filetype_version) {
        log.warning(
            "File '{}' is of a newer version than this software supports. "
            "Continuing, but errors may occur.",
            file_path);
    }

    if (file_header.cluster_data_offset != 0) {
        io::set_position(reader.value(), file_header.cluster_data_offset);
    }

    BlockScene::Clusters clusters;

    for (uint32_t cluster_index = 0; cluster_index < file_header.num_clusters; ++cluster_index) {
        ClusterHeader cluster_header{};
        io::read_binary(reader.value(), cluster_header);

        const ClusterCoords coords = { cluster_header.x, cluster_header.y };
        auto& new_cluster = clusters[coords];
        new_cluster.block_levels.resize(cluster_header.num_levels);

        if (cluster_header.level_data_offset != 0) {
            io::set_position(reader.value(), cluster_header.level_data_offset);
        }

        for (uint32_t level = 0; level < cluster_header.num_levels; ++level) {
            io::read_binary_array<Block>(reader.value(), new_cluster.block_levels[level]);
        }
    }

    return BlockScene{ file_header.block_size, std::move(clusters) };
}

} // namespace Mg
