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

#include <mg/gfx/mg_light_grid.h>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <mg/utils/mg_math_utils.h>

#include <iostream>

namespace Mg::gfx {

void LightGrid::calculate_delim_planes(glm::mat4 P)
{
    // If projection has not changed, we can re-use the previous planes
    if (P == m_prev_projection) { return; }
    m_prev_projection = P;

    glm::vec2 scale = glm::vec2{ MG_LIGHT_GRID_WIDTH, MG_LIGHT_GRID_HEIGHT } / 2.0f;

    // Create delimiter plane for each horizontal/vertical step by creating the frustum projection
    // matrix for that tile column/row, and then extracting frustum planes.
    for (size_t i = 0; i < MG_LIGHT_GRID_WIDTH + 1; ++i) {
        float tile_bias = -scale.x + i - 1;

        // Frustum projection matrix columns
        glm::vec4 col1{ P[0][0] * scale.x, 0.0f, tile_bias, 0.0f };
        glm::vec4 col4{ 0.0f, 0.0f, 1.0f, 0.0f };

        // Extract left frustum plane
        auto left             = glm::normalize(col4 + col1);
        m_delim_plane_vert[i] = DelimPlane{ left.x, left.z };
    }

    for (size_t u = 0; u < MG_LIGHT_GRID_HEIGHT + 1; ++u) {
        float tile_bias = -scale.y + u - 1;

        // Frustum projection matrix columns
        glm::vec4 col2{ 0.0f, P[1][1] * scale.y, tile_bias, 0.0f };
        glm::vec4 col4{ 0.0f, 0.0f, 1.0f, 0.0f };

        // Extract bottom frustum plane
        auto bottom          = glm::normalize(col4 + col2);
        m_delim_plane_hor[u] = DelimPlane{ bottom.y, bottom.z };
    }
}

size_t
LightGrid::extents(const glm::vec3& pos_view, float radius_sqr, bool horizontal, bool get_max)
{
    const int cmp_sign = get_max ? -1 : 1;

    auto planes = horizontal ? span{ m_delim_plane_hor } : span{ m_delim_plane_vert };

    const float offset = horizontal ? pos_view.y : pos_view.x;
    const float depth  = pos_view.z;

    size_t min = 0;
    size_t max = horizontal ? MG_LIGHT_GRID_HEIGHT : MG_LIGHT_GRID_WIDTH;

    if (signed_sqr_distance(planes[min], offset, depth) < cmp_sign * radius_sqr) { return 0; }
    if (signed_sqr_distance(planes[max], offset, depth) > cmp_sign * radius_sqr) { return max; }

    while (max - min > 1u) {
        const size_t      pivot       = min + (max - min) / 2;
        const DelimPlane& pivot_plane = planes[pivot]; // NOLINT

        const float dist_sqr_signed = signed_sqr_distance(pivot_plane, offset, depth);

        // Is light completely over delimiter
        if (dist_sqr_signed > cmp_sign * radius_sqr) { min = pivot; }
        else {
            max = pivot;
        }
    }

    return get_max ? max : min;
};

float LightGrid::signed_sqr_distance(const DelimPlane& plane, float offset, float depth)
{
    // Plane-point distance formula adapted for tile delimiter planes -- quite simplified,
    // since these planes are all normalised and always have components that are zero.
    const float distance = plane.A_or_B * offset + plane.C * depth;
    return sign(distance) * distance * distance;
};

} // namespace Mg::gfx
