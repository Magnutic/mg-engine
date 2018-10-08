//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_light_grid.h
 * Divides the view space frustum into a grid of tiles. Used for the Tiled Rendering and Clustered
 * Rendering techniques.
 */

#pragma once

#include <array>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <mg/mg_defs.h>

namespace Mg::gfx {

/* Divides the view space frustum into a grid of tiles. Used for the Tiled Rendering and Clustered
 * Rendering techniques.
 *
 * In tiled rendering, the screen is divided into a set of tiles (see MG_LIGHT_GRID_xxx, above). For
 * each tile, the set of light sources potentially affecting that tile is found and added to a list.
 * Then, the fragment shader calculates the tile in which it is located, and applies all lights
 * listed in said tile's light-list. This allows arbitrary number of lights in a scene in a manner
 * more efficient and flexible than per-object light-lists.
 *
 * Clustered rendering is an extension of tiled rendering, where depth is also taken into account --
 * each screen-space tile now corresponds to several projection-space clusters (see
 * MG_LIGHT_GRID_DEPTH). This allows more precise light lists, with less redundant light
 * calculations for tiles with large depth disparity.
 *
 * For more details, research 'Tiled Rendering' and 'Clustered Rendering'.
 * The original whitepaper is available here (as of March 2018):
 *     http://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf
 */
class LightGrid {
public:
    /** (Re-) calculate the tile-delimiter planes.
     * @param P camera projection matrix.
     */
    void calculate_delim_planes(glm::mat4 P);

    /** Find min or max extent of view space sphere within light grid. */
    size_t extents(const glm::vec3& pos_view, float radius_sqr, bool horizontal, bool get_max);

    /** Get extents of sphere in slice planes. (Used in clustered rendering, for tiled rendering,
     * extents() is sufficient.)
     */
    static std::pair<size_t, size_t> depth_extents(float depth, float radius)
    {
        static const float log2_max_dist = std::log2(float(MG_LIGHT_GRID_FAR_PLANE));
        constexpr float    grid_depth    = float(MG_LIGHT_GRID_DEPTH);
        constexpr float    grid_bias     = float(MG_LIGHT_GRID_DEPTH_BIAS);

        float fmin_z = (std::log2(depth - radius) / log2_max_dist) * grid_depth + grid_bias;
        float fmax_z = (std::log2(depth + radius) / log2_max_dist) * grid_depth + grid_bias;

        size_t min_z = size_t(std::max(0.0f, fmin_z));
        size_t max_z = size_t(std::max(0.0f, std::min(grid_depth - 1.0f, fmax_z))) + 1u;

        return { min_z, max_z };
    }

private:
    // View-space tile delimiter plane -- the planes that divide the screen into tiles.
    // Conventional plane representation (A*x + B*y + C*z -D == 0), but simplified for this
    // particular case. Since view space tile delimiters are always aligned on one axis, one of A or
    // B is always zero; and they all converge at camera position, meaning that D is always zero.
    //
    // TODO: consider orthographic projections -- for those, _all but_ D are zero.
    struct DelimPlane {
        float A_or_B; // A, in case delimiter is horizontal; otherwise B.
        float C;
    };

    /** Signed square distance between view-space position and tile delimiter plane.
     * @param plane The delimiter plane.
     * @param offset View-space offset (x, if delimiter plane is vertical; otherwise y).
     * @param depth View-space depth (z).
     */
    float signed_sqr_distance(const DelimPlane& plane, float offset, float depth);

    // View-space tile delimiter plane -- the planes that divide the screen into tiles.
    std::array<DelimPlane, MG_LIGHT_GRID_WIDTH + 1>  m_delim_plane_vert; // Facing negative x
    std::array<DelimPlane, MG_LIGHT_GRID_HEIGHT + 1> m_delim_plane_hor;  // Facing negative y

    // Cache camera projection matrix, so that we know whether tile-delimiter-planes need to be
    // re-calculated or not.
    glm::mat4 m_prev_projection{};
};

} // namespace Mg::gfx
