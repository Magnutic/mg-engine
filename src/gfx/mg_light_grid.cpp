//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_light_grid.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_math_utils.h"

namespace Mg::gfx {

void LightGrid::set_projection_matrix(glm::mat4 P) noexcept
{
    // If projection has not changed, we can re-use the previous planes
    if (P == m_prev_projection) {
        return;
    }
    m_prev_projection = P;

    // Otherwise, we have to recalculate the delimiter planes.
    const auto scale = glm::vec2{ m_config.grid_width, m_config.grid_height } / 2.0f;

    // Create delimiter plane for each horizontal/vertical step by creating the frustum projection
    // matrix for that tile column/row, and then extracting frustum planes.
    for (size_t i = 0; i < m_delim_plane_vert.size(); ++i) {
        const float tile_bias = -scale.x + float(i) - 1.0f;

        // Frustum projection matrix rows
        const glm::vec4 row1{ P[0][0] * scale.x, 0.0f, tile_bias, 0.0f /*P[3][0]*/ };
        // const glm::vec4 row4{ P[0][3], P[1][3], P[2][3], P[3][3] };
        const glm::vec4 row4{ 0.0f, 0.0f, 1.0f, 0.0f };

        // Extract left frustum plane
        const auto left = glm::normalize(row4 + row1);
        m_delim_plane_vert[i] = DelimPlane{ left.x, left.z };
    }

    for (size_t u = 0; u < m_delim_plane_hor.size(); ++u) {
        const float tile_bias = -scale.y + float(u) - 1.0f;

        // Frustum projection matrix rows
        const glm::vec4 row2{ 0.0f, P[1][1] * scale.y, tile_bias, 0.0f /*P[3][1]*/ };
        // const glm::vec4 row4{ P[0][3], P[1][3], P[2][3], P[3][3] };
        const glm::vec4 row4{ 0.0f, 0.0f, 1.0f, 0.0f };

        // Extract bottom frustum plane
        const auto bottom = glm::normalize(row4 + row2);
        m_delim_plane_hor[u] = DelimPlane{ bottom.y, bottom.z };
    }
}

LightGrid::TileExtents LightGrid::tile_extents(const glm::vec3& centre_viewspace,
                                               const float radius_sqr)
{
    return { { extents_impl(centre_viewspace, radius_sqr, ExtentAxis::X, Extremum::min),
               extents_impl(centre_viewspace, radius_sqr, ExtentAxis::Y, Extremum::min) },
             { extents_impl(centre_viewspace, radius_sqr, ExtentAxis::X, Extremum::max),
               extents_impl(centre_viewspace, radius_sqr, ExtentAxis::Y, Extremum::max) } };
}

size_t LightGrid::extents_impl(const glm::vec3& centre_viewspace,
                               const float radius_sqr,
                               const ExtentAxis axis,
                               const Extremum extremum) noexcept
{
    const float cmp_sign = (extremum == Extremum::max) ? -1.0f : 1.0f;
    const auto planes = (axis == ExtentAxis::Y) ? span{ m_delim_plane_hor }
                                                : span{ m_delim_plane_vert };
    const float offset = (axis == ExtentAxis::Y) ? centre_viewspace.y : centre_viewspace.x;
    const float depth = centre_viewspace.z;

    size_t min = 0;
    size_t max = (axis == ExtentAxis::Y) ? m_config.grid_height : m_config.grid_width;

    if (signed_sqr_distance(planes[min], offset, depth) < cmp_sign * radius_sqr) {
        return 0;
    }
    if (signed_sqr_distance(planes[max], offset, depth) > cmp_sign * radius_sqr) {
        return max;
    }

    while (max - min > 1u) {
        const size_t pivot = min + (max - min) / 2;
        const DelimPlane& pivot_plane = planes[pivot];

        const float dist_sqr_signed = signed_sqr_distance(pivot_plane, offset, depth);

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

float LightGrid::signed_sqr_distance(const DelimPlane& plane, float offset, float depth) noexcept
{
    // Plane-point distance formula adapted for tile delimiter planes -- quite simplified,
    // since these planes are all normalised and always have components that are zero.
    const float distance = plane.A_or_B * offset + plane.C * depth;
    return sign(distance) * distance * distance;
};

} // namespace Mg::gfx
