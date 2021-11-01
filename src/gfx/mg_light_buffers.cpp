//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_light_buffers.h"

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_log.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_light.h"
#include "mg/gfx/mg_light_grid_config.h"
#include "mg/mg_defs.h"
#include "mg_light_grid.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#    define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtx/fast_square_root.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <cstring> // memset, memcpy
#include <memory>
#include <type_traits>
#include <vector>

namespace Mg::gfx {

namespace {

// Types of buffers that will be uploaded to the GPU:

// Array of all light sources in the scene.
using LightBlock = std::vector<Light>;

// Index into LightBlock.
using LightIndex = uint16_t;

// An indirection table. Cluster points into this array, which in turn points into LightBlock.
using LightIndexArray = std::vector<LightIndex>;

// The 'header' for each cluster, telling how to find the lights that may affect this cluster.
struct Cluster {
    uint32_t offset_in_light_index_array;
    uint32_t num_lights_in_cluster;
};
static_assert(std::is_trivially_copyable_v<Cluster>);

using Clusters = std::vector<Cluster>;

// Working data used in preparing the buffers above.
struct ClusterWorkingDatum {
    small_vector<LightIndex, 32> light_indices;
};
using ClusterWorkingData = std::vector<ClusterWorkingDatum>;

constexpr BufferTexture::Type light_index_buffer_texture_type()
{
    BufferTexture::Type type{};
    type.channels = BufferTexture::Channels::R; // red: light index
    type.fmt = BufferTexture::Format::UNSIGNED;
    type.bit_depth = BufferTexture::BitDepth::BITS_16;
    return type;
}

constexpr BufferTexture::Type clusters_buffer_texture_type()
{
    BufferTexture::Type type{};
    // red: offset_in_light_index_array, green: num_lights_in_cluster.
    type.channels = BufferTexture::Channels::RG;
    type.fmt = BufferTexture::Format::UNSIGNED;
    type.bit_depth = BufferTexture::BitDepth::BITS_32;
    return type;
}

ClusterWorkingDatum& get_working_datum_for_cluster(glm::uvec3 cluster_cell,
                                                   ClusterWorkingData& working_data,
                                                   const LightGridConfig& grid_config)
{
    const auto cluster_index = grid_config.grid_width *
                                   (grid_config.grid_height * cluster_cell.z + cluster_cell.y) +
                               cluster_cell.x;
    MG_ASSERT(cluster_index < working_data.size());
    return working_data[cluster_index];
}

size_t num_clusters(const LightGridConfig& grid_config)
{
    return grid_config.grid_width * grid_config.grid_height * grid_config.grid_depth;
}

size_t light_block_buffer_size(const LightGridConfig& grid_config)
{
    return sizeof(Light) * grid_config.max_num_lights;
}

size_t light_index_buffer_size(const LightGridConfig& grid_config)
{
    return sizeof(LightIndexArray::value_type) * num_clusters(grid_config) *
           grid_config.max_lights_per_cluster;
}

size_t clusters_buffer_size(const LightGridConfig& grid_config)
{
    return sizeof(Clusters::value_type) * num_clusters(grid_config);
}

} // namespace

//--------------------------------------------------------------------------------------------------

struct LightBuffersData {
    explicit LightBuffersData(const LightGridConfig& config)
        : light_index_array(num_clusters(config) * config.max_lights_per_cluster)
        , clusters(num_clusters(config))
        , light_grid(config)
        , cluster_working_data(num_clusters(config))
    {}

    // Buffers that get uploaded to GPU.
    LightIndexArray light_index_array;
    Clusters clusters;

    // Local working data.
    LightGrid light_grid;
    ClusterWorkingData cluster_working_data;
};

LightBuffers::LightBuffers(const LightGridConfig& grid_config)
    : PImplMixin(grid_config)
    , light_block_buffer{ light_block_buffer_size(grid_config) }
    , light_index_texture{ light_index_buffer_texture_type(), light_index_buffer_size(grid_config) }
    , clusters_texture{ clusters_buffer_texture_type(), clusters_buffer_size(grid_config) }
{}

LightBuffers::~LightBuffers() = default;

void LightBuffers::update(span<const Light> lights, const ICamera& cam)
{
    MG_GFX_DEBUG_GROUP("update_light_data(LightBuffers&, ...)");
    LightBuffersData& m = impl();

    const LightGridConfig& grid_config = m.light_grid.config();
    const glm::mat4 V = cam.view_matrix();

    m.light_grid.set_projection_matrix(cam.proj_matrix());

    // Clear working data from previous updates.
    for (ClusterWorkingDatum& datum : m.cluster_working_data) {
        datum.light_indices.clear();
    }

    bool has_warned_about_too_many_lights = false;

    // Find which clusters each light intersects, and for each intersected cluster, write into the
    // corresponding cluster_working_data. This will be used below to build up the buffers which
    // will be uploaded to the GPU.
    for (size_t light_index = 0; light_index < lights.size(); ++light_index) {
        const Light& l = lights[light_index];

        // TODO handle directional lights.
        if (l.vector.w == 0.0) {
            continue;
        }

        const glm::vec3 light_pos_world = glm::vec3(l.vector);
        const glm::vec3 light_pos_view = V * glm::vec4(light_pos_world, 1.0f);

        // Early out if light is entirely behind camera
        if (light_pos_view.z > 0.0f && light_pos_view.z * light_pos_view.z >= l.range_sqr) {
            continue;
        }

        const auto [min, max] = m.light_grid.tile_extents(light_pos_view, l.range_sqr);
        const auto [min_z, max_z] = m.light_grid.depth_extents(-light_pos_view.z,
                                                               glm::fastSqrt(l.range_sqr));

        for (auto z = min_z; z < max_z; ++z) {
            for (auto y = min.y; y < max.y; ++y) {
                for (auto x = min.x; x < max.x; ++x) {
                    ClusterWorkingDatum& datum =
                        get_working_datum_for_cluster({ x, y, z },
                                                      m.cluster_working_data,
                                                      grid_config);
                    if (datum.light_indices.size() < grid_config.max_lights_per_cluster) {
                        datum.light_indices.push_back(narrow<uint16_t>(light_index));
                    }
                    else if (!has_warned_about_too_many_lights) {
                        log.warning("Too many light sources in cluster.");
                        has_warned_about_too_many_lights = true;
                    }
                }
            }
        }
    }

    // Clear buffers.
    const auto light_index_array_bytes = span{ m.light_index_array }.size_bytes();
    const auto tile_data_bytes = span{ m.clusters }.size_bytes();
    std::memset(m.light_index_array.data(), 0, light_index_array_bytes);
    std::memset(m.clusters.data(), 0, tile_data_bytes);

    // Prepare cluster and light_index buffers to uploading to GPU.
    MG_ASSERT(m.cluster_working_data.size() == m.clusters.size());
    uint32_t current_light_index_buffer_index = 0;
    for (size_t cluster_index = 0; cluster_index < m.clusters.size(); ++cluster_index) {
        const ClusterWorkingDatum& datum = m.cluster_working_data[cluster_index];
        Cluster& cluster = m.clusters[cluster_index];

        // Write the offset into the LightIndex array for this cluster.
        cluster.offset_in_light_index_array = current_light_index_buffer_index;
        cluster.num_lights_in_cluster = narrow<uint32_t>(datum.light_indices.size());

        for (LightIndex light_index : datum.light_indices) {
            m.light_index_array[current_light_index_buffer_index] = light_index;
            ++current_light_index_buffer_index;
        }
    }

    // Upload to GPU.
    light_block_buffer.set_data(lights.as_bytes());
    clusters_texture.set_data(as_bytes(span{ m.clusters }));
    light_index_texture.set_data(as_bytes(span{ m.light_index_array }));
}

const LightGridConfig& LightBuffers::config() const
{
    return impl().light_grid.config();
}

} // namespace Mg::gfx
