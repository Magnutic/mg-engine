//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_light_grid.h
 * Divides the view space frustum into a grid of tiles. Used for the Tiled Rendering and Clustered
 * Rendering techniques.
 */

#pragma once

#include "mg/gfx/mg_light_grid_config.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace Mg::gfx {

/* Divides the view-space frustum into a grid of tiles. Used for the Tiled Rendering and Clustered
 * Rendering techniques. Mg Engine supports clustered rendering.
 *
 * In tiled rendering, the screen is divided into a set of tiles (see LightGridConfig). For
 * each tile, the set of light sources potentially affecting that tile is found and added to a list.
 * Then, the fragment shader calculates the tile in which it is located, and applies all lights
 * listed in said tile's light-list. This allows arbitrary number of lights in a scene in a manner
 * more efficient and flexible than per-object light-lists.
 *
 * Clustered rendering is an extension of tiled rendering, where depth is also taken into account --
 * each screen-space tile now corresponds to several projection-space clusters (see
 * LightGridConfig::grid_depth). This allows more precise light lists, with less redundant light
 * calculations for tiles with large depth disparity.
 *
 * This class provides methods for finding the tile and cluster extents of light sources. Given
 * a light source in view space, `extents` will return which tiles it intersects, and
 * `depth_extents` will tell which clusters within those tiles it intersects. The delimiter planes
 * used in these calculations must be up to date with the used projection matrix, see
 * `set_projection_matrix`.
 *
 * For more details, research 'Tiled Rendering' and 'Clustered Rendering'.
 * The original whitepaper is available here (as of March 2018):
 *     http://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf
 */
class LightGrid {
public:
    explicit LightGrid(const LightGridConfig& config) : m_config(config)
    {
        allocate_delim_planes();
    }

    /** Set the projection matrix to which future extents queries should apply. This will
     * (re-)calculate the tile-delimiter planes, if needed.
     * @param P camera projection matrix.
     */
    void set_projection_matrix(glm::mat4 P) noexcept;

    struct TileExtents {
        glm::ivec2 min;
        glm::ivec2 max;
    };

    /** Get extents of view-sphere in delimiter planes, which defines a rectangular region of tiles
     * such that the all tiles intersecting the sphere will be included.
     */
    TileExtents tile_extents(const glm::vec3& centre_viewspace, float radius_sqr);

    /** Get extents of sphere in delimiter planes. (Used in clustered rendering; for tiled
     * rendering, tile_extents() is sufficient.)
     */
    std::pair<size_t, size_t> depth_extents(const float depth, const float radius) noexcept
    {
        const auto log2_max_dist = std::log2(float(m_config.grid_far_plane));
        const auto grid_depth_f = float(m_config.grid_depth);

        const float fmin_z = (std::log2(depth - radius) / log2_max_dist) * grid_depth_f +
                             m_config.depth_bias;
        const float fmax_z = (std::log2(depth + radius) / log2_max_dist) * grid_depth_f +
                             m_config.depth_bias;

        const auto min_z = size_t(std::max(0.0f, fmin_z));
        const auto max_z = size_t(std::max(0.0f, std::min(grid_depth_f - 1.0f, fmax_z))) + 1u;

        return { min_z, max_z };
    }

    const LightGridConfig& config() const { return m_config; }

    void config(const LightGridConfig& new_config)
    {
        const bool grid_size_changed = new_config.grid_width != m_config.grid_width ||
                                       new_config.grid_height != m_config.grid_height;
        if (grid_size_changed) {
            allocate_delim_planes();
        }
    }

private:
    enum class ExtentAxis { X, Y };
    enum class Extremum { min, max };

    // Find min or max extent of view-space sphere within light grid.
    size_t extents_impl(const glm::vec3& centre_viewspace,
                        float radius_sqr,
                        ExtentAxis axis,
                        Extremum extremum) noexcept;

    // View-space tile delimiter plane -- the planes that divide the screen into tiles.
    // Conventional plane representation (A*x + B*y + C*z -D == 0), but simplified for this
    // particular case. Since view space tile delimiters are always aligned on one axis, one of A or
    // B is always zero; and they all converge at camera position, meaning that D is always zero.
    struct DelimPlane {
        float A_or_B = 0.0f;
        float C = 0.0f;
    };

    // The configuration for this light grid.
    LightGridConfig m_config;

    /** Signed square distance between view-space position and tile delimiter plane.
     * @param plane The delimiter plane.
     * @param offset View-space offset (x, if delimiter plane is vertical; otherwise y).
     * @param depth View-space depth (z).
     */
    float signed_sqr_distance(const DelimPlane& plane, float offset, float depth) noexcept;

    void allocate_delim_planes()
    {
        m_delim_plane_vert.resize(m_config.grid_width + 1);
        m_delim_plane_hor.resize(m_config.grid_height + 1);
    }

    // View-space tile delimiter plane -- the planes that divide the screen into tiles.
    std::vector<DelimPlane> m_delim_plane_vert; // Facing negative x
    std::vector<DelimPlane> m_delim_plane_hor;  // Facing negative y

    // Cache camera projection matrix, so that we know whether tile-delimiter planes need to be
    // re-calculated or not.
    glm::mat4 m_prev_projection{};
};

} // namespace Mg::gfx
