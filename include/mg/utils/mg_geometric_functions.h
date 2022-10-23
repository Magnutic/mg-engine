//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_geometric_functions.h
 * Functions for calculations on geometric data.
 */

#pragma once

#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_angle.h"
#include "mg/utils/mg_gsl.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace Mg {

/** Non-owning view over data for a polygon. */
struct PolygonDataView {
    span<const gfx::Mesh::Vertex> vertices;
    span<const gfx::Mesh::Index> polygon;
};

/** Get position for a vertex in the polygon. */
inline glm::vec3 position(const PolygonDataView& data, gfx::Mesh::Index index)
{
    return data.vertices[index].position;
}

/** Get signed angle between line bc to ba. */
inline Angle vertex_angle(const PolygonDataView& data,
                          gfx::Mesh::Index a,
                          gfx::Mesh::Index b,
                          gfx::Mesh::Index c)
{
    const auto& pos_a = data.vertices[a].position;
    const auto& pos_b = data.vertices[b].position;
    const auto& pos_c = data.vertices[c].position;

    const auto ba = glm::normalize(pos_a - pos_b);
    const auto bc = glm::normalize(pos_c - pos_b);
    const auto shortest_angle = glm::angle(bc, ba);
    const auto determinant = bc.x * ba.y - bc.y * ba.x;
    const auto angle = (determinant < 0.0f) ? -shortest_angle : shortest_angle;
    return Angle::from_radians(angle);
}

/** Previous index, with wrap around. */
template<typename T, typename U> T previous_index(const T index, const U num_indices)
{
    MG_ASSERT_DEBUG(index >= T(0));
    return index == 0 ? (num_indices > 0 ? as<T>(num_indices - 1) : 0) : index - 1;
}

/** Next index, with wrap around. */
template<typename T, typename U> T next_index(const T index, const U num_indices)
{
    MG_ASSERT_DEBUG(index >= T(0));
    return (index + 1) % as<T>(num_indices);
}

/** Orientation a set of points. */
enum Orientation { Clockwise, Counter_clockwise, Collinear };

/** Get orientation of triangle. */
inline Orientation triangle_orientation(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const auto slope_diff = (b.y - a.y) * (c.x - b.x) - (c.y - b.y) * (b.x - a.x);
    if (slope_diff == 0.0f) {
        return Orientation::Collinear;
    }
    return slope_diff > 0.0f ? Orientation::Clockwise : Orientation::Counter_clockwise;
}

/** Winding order of a polygon. */
enum class Winding { Clockwise, Counter_clockwise };

/** Calculate winding order for a polygon. */
Winding calculate_polygon_winding(const PolygonDataView& data);

/** Assuming the three points are collinear, does point intersect the lines? */
inline bool collinear_point_on_line(const glm::vec3& point,
                                    const glm::vec3& line_start,
                                    const glm::vec3& line_end)
{
    return point.x > std::min(line_start.x, line_end.x) &&
           point.x < std::max(line_start.x, line_end.x) &&
           point.y > std::min(line_start.y, line_end.y) &&
           point.y < std::max(line_start.y, line_end.y);
}

/** Get whether the two given line segments intersect. */
bool line_segments_intersect(glm::vec3 start_1,
                             glm::vec3 end_1,
                             glm::vec3 start_2,
                             glm::vec3 end_2);

/** Get whether the two edges, which would be formed by connecting two pairs of vertices in
 * a polygon, would intersect.
 */
bool edges_intersect(const PolygonDataView& data,
                     gfx::Mesh::Index start_1_index,
                     gfx::Mesh::Index end_1_index,
                     gfx::Mesh::Index start_2_index,
                     gfx::Mesh::Index end_2_index);

/** Get whether a vertex within a polygon is convex. */
inline bool is_convex_vertex(const PolygonDataView& data,
                             const gfx::Mesh::Index prev,
                             const gfx::Mesh::Index current,
                             const gfx::Mesh::Index next)
{
    return vertex_angle(data, prev, current, next).radians() < 0.0f;
}

/** Triangulation using the ear-clipping algorithm. */
void calculate_triangles(PolygonDataView data, std::vector<gfx::Mesh::Index>& triangles_out);

} // namespace Mg
