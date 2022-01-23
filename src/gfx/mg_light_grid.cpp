//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_light_grid.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_math_utils.h"

// Shortcut to avoid redundant computation. The non-shortcut version exists mostly for expository
// reasons.
#define MG_LIGHT_GRID_USE_DELIMITER_PLANE_CALCULATION_SHORTCUT 1

namespace Mg::gfx {

void LightGrid::set_projection_matrix(glm::mat4 P) noexcept
{
    // If projection has not changed, we can re-use the previous planes
    if (P == m_prev_projection) {
        return;
    }

    // Otherwise, we have to recalculate the delimiter planes.
    m_prev_projection = P;

#if MG_LIGHT_GRID_USE_DELIMITER_PLANE_CALCULATION_SHORTCUT
    const auto sx = float(m_config.grid_width);
    const auto sy = float(m_config.grid_height);
#endif

    // Create delimiter plane for each horizontal/vertical step by creating the frustum projection
    // matrix for that tile column/row, and then extracting frustum planes.
    for (size_t i = 0; i < m_delim_plane_vert.size(); ++i) {
#if MG_LIGHT_GRID_USE_DELIMITER_PLANE_CALCULATION_SHORTCUT
        const auto tx = 1.0f - (2.0f * float(i) + 1.0f) / sx;

        // Frustum projection matrix rows
        const glm::vec4 row0{ P[0][0] * sx + P[0][3] * tx * sx,
                              P[1][0] * sx + P[1][3] * tx * sx,
                              P[2][0] * sx + P[2][3] * tx * sx,
                              P[3][0] * sx + P[3][3] * tx * sx };

        const glm::vec4 row3{ P[0][3], P[1][3], P[2][3], P[3][3] };
#else
        glm::mat4 M = clip_space_to_tile_space(m_config, { i, 0 }) * P;
        const glm::vec4 row0{ M[0][0], M[1][0], M[2][0], M[3][0] };
        const glm::vec4 row3{ M[0][3], M[1][3], M[2][3], M[3][3] };
#endif

        // Extract left frustum plane
        const auto left = row3 + row0;
        m_delim_plane_vert[i] = PointNormalPlane::from_coefficients(left);
    }

    for (size_t u = 0; u < m_delim_plane_hor.size(); ++u) {
#if MG_LIGHT_GRID_USE_DELIMITER_PLANE_CALCULATION_SHORTCUT
        const auto ty = 1.0f - (2.0f * float(u) + 1.0f) / sy;

        // Frustum projection matrix rows
        const glm::vec4 row1{ P[0][1] * sy + P[0][3] * ty * sy,
                              P[1][1] * sy + P[1][3] * ty * sy,
                              P[2][1] * sy + P[2][3] * ty * sy,
                              P[3][1] * sy + P[3][3] * ty * sy };

        const glm::vec4 row3{ P[0][3], P[1][3], P[2][3], P[3][3] };
#else
        glm::mat4 M = clip_space_to_tile_space(m_config, { 0, u }) * P;
        const glm::vec4 row1{ M[0][1], M[1][1], M[2][1], M[3][1] };
        const glm::vec4 row3{ M[0][3], M[1][3], M[2][3], M[3][3] };
#endif

        // Extract bottom frustum plane
        const auto bottom = row3 + row1;
        m_delim_plane_hor[u] = PointNormalPlane::from_coefficients(bottom);
    }
}

glm::mat4 LightGrid::clip_space_to_tile_space(const LightGridConfig& config, const glm::ivec2& tile)
{
    // Scaling from tile-centered clip space to tile space.
    const auto sx = float(config.grid_width);
    const auto sy = float(config.grid_height);

    // Translation from clip space to a space centered on the tile.
    const auto tx = 1.0f - (2.0f * float(tile.x) + 1.0f) / float(config.grid_width);
    const auto ty = 1.0f - (2.0f * float(tile.y) + 1.0f) / float(config.grid_height);

    // Matrix concatenating the above transformations.
    return { { sx, 0.0f, 0.0f, 0.0f },
             { 0.0f, sy, 0.0f, 0.0f },
             { 0.0f, 0.0f, 1.0f, 0.0f },
             { tx * sx, ty * sy, 0.0f, 1.0f } };
}

LightGrid::TileExtents LightGrid::tile_extents(const glm::vec3& centre_viewspace,
                                               const float radius_sqr)
{
    return { { extents_impl(centre_viewspace, radius_sqr, ExtentAxis::X, Extremum::min),
               extents_impl(centre_viewspace, radius_sqr, ExtentAxis::Y, Extremum::min) },
             { extents_impl(centre_viewspace, radius_sqr, ExtentAxis::X, Extremum::max),
               extents_impl(centre_viewspace, radius_sqr, ExtentAxis::Y, Extremum::max) } };
}

static float signed_sqr_distance(const PointNormalPlane& plane, const glm::vec3& p) noexcept
{
    const auto d = signed_distance_to_plane(plane, p);
    return sign(d) * d * d;
}

size_t LightGrid::extents_impl(const glm::vec3& centre_viewspace,
                               const float radius_sqr,
                               const ExtentAxis axis,
                               const Extremum extremum) noexcept
{
    const float cmp_sign = (extremum == Extremum::max) ? -1.0f : 1.0f;
    const auto& planes = (axis == ExtentAxis::Y) ? m_delim_plane_hor : m_delim_plane_vert;

    size_t min = 0;
    size_t max = (axis == ExtentAxis::Y) ? m_config.grid_height : m_config.grid_width;

    if (signed_sqr_distance(planes[min], centre_viewspace) < cmp_sign * radius_sqr) {
        return 0;
    }
    if (signed_sqr_distance(planes[max], centre_viewspace) > cmp_sign * radius_sqr) {
        return max;
    }

    while (max - min > 1u) {
        const size_t pivot = min + (max - min) / 2;
        const PointNormalPlane& pivot_plane = planes[pivot];

        const float dist_sqr_signed = signed_sqr_distance(pivot_plane, centre_viewspace);

        // Is light completely over delimiter
        if (dist_sqr_signed > cmp_sign * radius_sqr) {
            min = pivot;
        }
        else {
            max = pivot;
        }
    }

    return (extremum == Extremum::max) ? max : min;
};

} // namespace Mg::gfx
