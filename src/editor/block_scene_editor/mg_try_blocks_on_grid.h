//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2026, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_try_blocks_on_grid.h
 * Higher-order function for performing an operation on each block intersecting a ray.
 */

#pragma once

#include <glm/vec3.hpp>

#include <cfloat>
#include <cmath>
#include <concepts>
#include <cstdlib>

namespace Mg {

/** Invoke the given function on each block intersecting a given ray, stopping when the max distance
 * has been reached or the function returns `true`.
 *
 * @param start Start coordinate of the ray (world space)
 * @param ray_direction Direction of the ray (normalized)
 * @param function The function to invoke on each block.
 * @param block_size How large each block is in world-space units (depends on block-scene config).
 * @param max_distance How far along the ray to travel.
 */
template<std::invocable<int /*grid_x*/,
                        int /*grid_y*/,
                        float /*z_on_cell_entry*/,
                        float /*z_on_cell_exit*/> F>
void try_blocks_on_grid(const glm::vec3 start,
                        const glm::vec3 ray_direction,
                        F&& function,
                        const float block_size,
                        const float max_distance)
{
    const auto v = ray_direction;

    const int step_x = v.x > 0.0f ? 1 : -1;
    const int step_y = v.y > 0.0f ? 1 : -1;

    const auto t_delta_x = std::abs(v.x) > FLT_EPSILON ? std::abs(1.0f / v.x) : FLT_MAX;
    const auto t_delta_y = std::abs(v.y) > FLT_EPSILON ? std::abs(1.0f / v.y) : FLT_MAX;

    const auto start_in_grid_units = start / block_size;

    auto cell_x = int(std::floor(start_in_grid_units.x));
    auto cell_y = int(std::floor(start_in_grid_units.y));

    auto t_max_x = v.x < 0.0f ? (start_in_grid_units.x - float(cell_x)) * t_delta_x
                              : (1.0f - start_in_grid_units.x + float(cell_x)) * t_delta_x;
    auto t_max_y = v.y < 0.0f ? (start_in_grid_units.y - float(cell_y)) * t_delta_y
                              : (1.0f - start_in_grid_units.y + float(cell_y)) * t_delta_y;

    auto cell_entry_t = 0.0f;
    for (;;) {
        if (cell_entry_t * block_size > max_distance) {
            return;
        }

        const auto cell_exit_t = std::min(t_max_x, t_max_y);
        const auto z_on_cell_entry = start.z + (cell_entry_t * block_size * v).z;
        const auto z_on_cell_exit = start.z + (cell_exit_t * block_size * v).z;
        if (function(cell_x, cell_y, z_on_cell_entry, z_on_cell_exit)) {
            return;
        }

        if (t_max_x < t_max_y) {
            cell_entry_t = t_max_x;
            t_max_x += t_delta_x;
            cell_x += step_x;
        }
        else {
            cell_entry_t = t_max_y;
            t_max_y += t_delta_y;
            cell_y += step_y;
        }
    }
}


} // namespace Mg
