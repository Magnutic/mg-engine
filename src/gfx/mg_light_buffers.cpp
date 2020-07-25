//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_light_buffers.h"

#include "mg_light_grid.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/mg_defs.h"
#include <mg/gfx/mg_light.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#    define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtx/fast_square_root.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstdint>
#include <cstring> // memset, memcpy
#include <memory>

namespace Mg::gfx {

namespace {

using LightBlock = std::array<Light, MG_MAX_NUM_LIGHTS>;

constexpr size_t k_max_ubo_size = 16 * 1024;

// Keep below lower bound on max UBO size. TODO: split into multiple UBOs if too large?
static_assert(sizeof(LightBlock) <= k_max_ubo_size);

constexpr BufferTexture::Type light_index_tex_type()
{
    BufferTexture::Type type{};
    type.channels = BufferTexture::Channels::R; // red: light index
    type.fmt = BufferTexture::Format::UNSIGNED;
    type.bit_depth = BufferTexture::BitDepth::BITS_16;
    return type;
}

constexpr BufferTexture::Type light_grid_tex_type()
{
    BufferTexture::Type type{};
    type.channels = BufferTexture::Channels::RG; // red: offset, green: amount.
    type.fmt = BufferTexture::Format::UNSIGNED;
    type.bit_depth = BufferTexture::BitDepth::BITS_32;
    return type;
}

struct ClusterData {
    uint16_t num_lights;
    std::array<uint16_t, MG_MAX_LIGHTS_PER_CLUSTER> light_indices;
};

using ClusterArray = std::array<ClusterData, MG_LIGHT_GRID_SIZE>;
using LightIndexArray = std::array<uint16_t, MG_LIGHT_GRID_SIZE * MG_MAX_LIGHTS_PER_CLUSTER>;
using LightGridData = std::array<uint32_t, MG_LIGHT_GRID_SIZE * 2u>;

static_assert(std::is_trivially_copyable_v<ClusterArray>);
static_assert(std::is_trivially_copyable_v<LightIndexArray>);
static_assert(std::is_trivially_copyable_v<LightGridData>);

void add_light_to_cluster(size_t light_index, glm::uvec3 cluster, ClusterArray& clusters) noexcept
{
    auto cluster_index = MG_LIGHT_GRID_WIDTH * (MG_LIGHT_GRID_HEIGHT * cluster.z + cluster.y) +
                         cluster.x;
    const auto light_offset = clusters[cluster_index].num_lights;

    if (light_offset >= MG_MAX_LIGHTS_PER_CLUSTER) {
        g_log.write_warning("Too many light sources in cluster.");
        return;
    }

    clusters[cluster_index].light_indices[light_offset] = narrow<uint16_t>(light_index);
    ++clusters[cluster_index].num_lights;
}

// Reserve memory for the temporary buffers within which update_light_data creates the data to
// upload to the GPU. These are reserved and re-used between invocations of update_light_data to
// avoid costly large dynamic memory allocations -- putting them on the stack would not work, as the
// required memory is too large.
auto clusters = std::make_unique<ClusterArray>();
auto light_index_array = std::make_unique<LightIndexArray>();
auto light_grid_data = std::make_unique<LightGridData>();

} // namespace

//--------------------------------------------------------------------------------------------------

LightBuffers::LightBuffers() noexcept
    : light_data_buffer{ sizeof(LightBlock) }
    , light_index_texture{ light_index_tex_type(), sizeof(LightIndexArray) }
    , tile_data_texture{ light_grid_tex_type(), sizeof(LightGridData) }
{}

void update_light_data(LightBuffers& light_data_out,
                       span<const Light> lights,
                       const ICamera& cam,
                       LightGrid& grid)
{
    MG_GFX_DEBUG_GROUP("update_light_data(LightBuffers&, ...)");

    std::memset(clusters.get(), 0, sizeof(ClusterArray)); // Reset cluster data

    grid.calculate_delim_planes(cam.proj_matrix());

    const glm::mat4 V = cam.view_matrix();

    for (size_t light_index = 0; light_index < lights.size(); ++light_index) {
        const Light& l = lights[light_index];

        if (l.vector.w == 0.0) {
            continue;
        }

        const glm::vec3 light_pos_world = glm::vec3(l.vector);
        const glm::vec3 light_pos_view = V * glm::vec4(light_pos_world, 1.0f);

        // Early out if light is entirely behind camera
        if (light_pos_view.z > 0.0f && light_pos_view.z * light_pos_view.z >= l.range_sqr) {
            continue;
        }

        const size_t min_x = grid.extents(light_pos_view, l.range_sqr, false, false);
        const size_t max_x = grid.extents(light_pos_view, l.range_sqr, false, true);
        const size_t min_y = grid.extents(light_pos_view, l.range_sqr, true, false);
        const size_t max_y = grid.extents(light_pos_view, l.range_sqr, true, true);

        const auto [min_z, max_z] = LightGrid::depth_extents(-light_pos_view.z,
                                                             glm::fastSqrt(l.range_sqr));

        for (auto z = min_z; z < max_z; ++z) {
            for (auto y = min_y; y < max_y; ++y) {
                for (auto x = min_x; x < max_x; ++x) {
                    add_light_to_cluster(light_index, glm::uvec3(x, y, z), *clusters);
                }
            }
        }
    }

    std::memset(light_index_array.get(), 0, sizeof(LightIndexArray));
    std::memset(light_grid_data.get(), 0, sizeof(LightGridData));

    uint32_t light_index_accumulator = 0;

    for (size_t cluster_index = 0; cluster_index < clusters->size(); ++cluster_index) {
        const ClusterData& cluster = (*clusters)[cluster_index];
        (*light_grid_data)[cluster_index * 2u] = light_index_accumulator;

        for (uint16_t i = 0; i < cluster.num_lights; ++i) {
            auto light_index = cluster.light_indices[i];

            (*light_index_array)[light_index_accumulator++] = light_index;
            ++((*light_grid_data)[cluster_index * 2u + 1u]);
        }
    }

    light_data_out.light_data_buffer.set_data(lights.as_bytes());
    light_data_out.tile_data_texture.set_data(byte_representation(*light_grid_data));
    light_data_out.light_index_texture.set_data(byte_representation(*light_index_array));
}

} // namespace Mg::gfx
