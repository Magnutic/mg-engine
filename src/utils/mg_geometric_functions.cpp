//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/utils/mg_geometric_functions.h"

namespace Mg {

Winding calculate_polygon_winding(const PolygonDataView& data)
{
    const auto num_vertices = data.polygon.size();
    uint32_t selected_index = 0;

    for (uint32_t i = 1; i < num_vertices; ++i) {
        const auto pos = data.vertices[data.polygon[i]].position;
        const auto selected_pos = data.vertices[data.polygon[selected_index]].position;

        if (pos.y < selected_pos.y || (pos.y == selected_pos.y && pos.x > selected_pos.x)) {
            selected_index = i;
        }
    }

    const uint32_t prev = previous_index(selected_index, num_vertices);
    const uint32_t next = next_index(selected_index, num_vertices);

    const auto prev_pos = data.vertices[data.polygon[prev]].position;
    const auto pos = data.vertices[data.polygon[selected_index]].position;
    const auto next_pos = data.vertices[data.polygon[next]].position;

    const auto determinant = (pos.x * next_pos.y + prev_pos.x * pos.y + prev_pos.y * next_pos.x) -
                             (prev_pos.y * pos.x + pos.y * next_pos.x + prev_pos.x * next_pos.y);

    return determinant > 0.0f ? Winding::Counter_clockwise : Winding::Clockwise;
}

bool line_segments_intersect(const glm::vec3 start_1,
                             const glm::vec3 end_1,
                             const glm::vec3 start_2,
                             const glm::vec3 end_2)
{
    const auto o1 = triangle_orientation(start_1, end_1, start_2);
    const auto o2 = triangle_orientation(start_1, end_1, end_2);
    const auto o3 = triangle_orientation(start_2, end_2, start_1);
    const auto o4 = triangle_orientation(start_2, end_2, end_1);

    if (o1 != o2 && o3 != o4) {
        return true;
    }

    if (o1 == Orientation::Collinear) {
        return collinear_point_on_line(start_2, start_1, end_1);
    }
    if (o2 == Orientation::Collinear) {
        return collinear_point_on_line(end_2, start_1, end_1);
    }
    if (o3 == Orientation::Collinear) {
        return collinear_point_on_line(start_1, start_2, end_2);
    }
    if (o4 == Orientation::Collinear) {
        return collinear_point_on_line(end_1, start_2, end_2);
    }

    return false;
}

bool edges_intersect(const PolygonDataView& data,
                     const gfx::mesh_data::Index start_1_index,
                     const gfx::mesh_data::Index end_1_index,
                     const gfx::mesh_data::Index start_2_index,
                     const gfx::mesh_data::Index end_2_index)
{
    const auto start_1 = position(data, start_1_index);
    const auto end_1 = position(data, end_1_index);
    const auto start_2 = position(data, start_2_index);
    const auto end_2 = position(data, end_2_index);
    return line_segments_intersect(start_1, end_1, start_2, end_2);
}

void calculate_triangles(PolygonDataView data, std::vector<gfx::mesh_data::Index>& triangles_out)
{
    if (data.polygon.size() < 3) {
        return;
    }

    std::vector<std::pair<gfx::mesh_data::Index, bool /* is_ear */>> unhandled;
    unhandled.reserve(data.polygon.size());
    for (const auto index : data.polygon) {
        unhandled.emplace_back(index, false);
    }

    auto calculate_is_ear = [&](const uint32_t unhandled_index) {
        const size_t num_unhandled = unhandled.size();

        const uint32_t prev = previous_index(unhandled_index, num_unhandled);
        const uint32_t next = next_index(unhandled_index, num_unhandled);

        const gfx::mesh_data::Index prev_vertex_index = unhandled[prev].first;
        const gfx::mesh_data::Index this_vertex_index = unhandled[unhandled_index].first;
        const gfx::mesh_data::Index next_vertex_index = unhandled[next].first;

        if (triangle_orientation(data.vertices[prev_vertex_index].position,
                                 data.vertices[this_vertex_index].position,
                                 data.vertices[next_vertex_index].position) !=
            Orientation::Counter_clockwise) {
            return false;
        }

        for (uint32_t i = 0; i < num_unhandled; ++i) {
            const uint32_t edge_start = i;
            const uint32_t edge_end = next_index(i, num_unhandled);
            if (edge_start == prev || edge_end == prev || edge_start == next || edge_end == next) {
                continue;
            }

            const gfx::mesh_data::Index edge_start_vertex_index = unhandled[edge_start].first;
            const gfx::mesh_data::Index edge_end_vertex_index = unhandled[edge_end].first;

            if (edges_intersect(data,
                                prev_vertex_index,
                                next_vertex_index,
                                edge_start_vertex_index,
                                edge_end_vertex_index)) {
                return false;
            }
        }

        return true;
    };

    for (uint32_t i = 0; i < unhandled.size(); ++i) {
        unhandled[i].second = calculate_is_ear(i);
    }

    while (unhandled.size() > 3) {
        const auto num_unhandled = unhandled.size();

        // Index into remaining_vertices.
        Opt<uint32_t> ear_index;

        for (uint32_t i = 0; i < num_unhandled; ++i) {
            const auto [index, is_ear] = unhandled[i];
            if (is_ear) {
                ear_index = i;
                break;
            }
        }

        MG_ASSERT(ear_index.has_value());

        const auto prev = previous_index(*ear_index, num_unhandled);
        const auto next = next_index(*ear_index, num_unhandled);
        triangles_out.push_back(unhandled[prev].first);
        triangles_out.push_back(unhandled[*ear_index].first);
        triangles_out.push_back(unhandled[next].first);

        // Remove ear from unhandled.
        unhandled.erase(unhandled.begin() + *ear_index);

        // Update is_ear status of adjacent vertices.
        unhandled[prev].second = calculate_is_ear(prev);
        unhandled[*ear_index].second = calculate_is_ear(*ear_index);
    }

    if (unhandled.size() == 3) {
        triangles_out.push_back(unhandled[0].first);
        triangles_out.push_back(unhandled[1].first);
        triangles_out.push_back(unhandled[2].first);
    }
}

} // namespace Mg
