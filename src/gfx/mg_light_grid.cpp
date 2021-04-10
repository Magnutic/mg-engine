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

#include "mg/utils/mg_math_utils.h"

#include <iostream>

namespace Mg::gfx {

void LightGrid::calculate_delim_planes(glm::mat4 P) noexcept
{
    // If projection has not changed, we can re-use the previous planes
    if (P == m_prev_projection) {
        return;
    }
    m_prev_projection = P;

    const auto scale = glm::vec2{ defs::light_grid_width, defs::light_grid_height } / 2.0f;

    // Create delimiter plane for each horizontal/vertical step by creating the frustum projection
    // matrix for that tile column/row, and then extracting frustum planes.
    for (size_t i = 0; i < defs::light_grid_width + 1; ++i) {
        const float tile_bias = -scale.x + float(i) - 1.0f;

        // Frustum projection matrix columns
        const glm::vec4 col1{ P[0][0] * scale.x, 0.0f, tile_bias, 0.0f };
        const glm::vec4 col4{ 0.0f, 0.0f, 1.0f, 0.0f };

        // Extract left frustum plane
        const auto left = glm::normalize(col4 + col1);
        m_delim_plane_vert[i] = DelimPlane{ left.x, left.z };
    }

    for (size_t u = 0; u < defs::light_grid_height + 1; ++u) {
        const float tile_bias = -scale.y + float(u) - 1.0f;

        // Frustum projection matrix columns
        const glm::vec4 col2{ 0.0f, P[1][1] * scale.y, tile_bias, 0.0f };
        const glm::vec4 col4{ 0.0f, 0.0f, 1.0f, 0.0f };

        // Extract bottom frustum plane
        const auto bottom = glm::normalize(col4 + col2);
        m_delim_plane_hor[u] = DelimPlane{ bottom.y, bottom.z };
    }
}

size_t LightGrid::extents(const glm::vec3& pos_view,
                          float radius_sqr,
                          bool horizontal,
                          bool get_max) noexcept
{
    const float cmp_sign = get_max ? -1.0f : 1.0f;

    const auto planes = horizontal ? span{ m_delim_plane_hor } : span{ m_delim_plane_vert };

    const float offset = horizontal ? pos_view.y : pos_view.x;
    const float depth = pos_view.z;

    size_t min = 0;
    size_t max = horizontal ? defs::light_grid_height : defs::light_grid_width;

    if (signed_sqr_distance(planes[min], offset, depth) < cmp_sign * radius_sqr) {
        return 0;
    }
    if (signed_sqr_distance(planes[max], offset, depth) > cmp_sign * radius_sqr) {
        return max;
    }

    while (max - min > 1u) {
        const size_t pivot = min + (max - min) / 2;
        const DelimPlane& pivot_plane = planes[pivot]; // NOLINT

        const float dist_sqr_signed = signed_sqr_distance(pivot_plane, offset, depth);

        // Is light completely over delimiter
        if (dist_sqr_signed > cmp_sign * radius_sqr) {
            min = pivot;
        }
        else {
            max = pivot;
        }
    }

    return get_max ? max : min;
};

float LightGrid::signed_sqr_distance(const DelimPlane& plane, float offset, float depth) noexcept
{
    // Plane-point distance formula adapted for tile delimiter planes -- quite simplified,
    // since these planes are all normalised and always have components that are zero.
    const float distance = plane.A_or_B * offset + plane.C * depth;
    return sign(distance) * distance * distance;
};

} // namespace Mg::gfx
