#include "mg/mg_bounding_volumes.h"

#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_math_utils.h"

#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/vec3.hpp>

#include <numeric>

namespace Mg {

using namespace Mg::gfx::Mesh;

namespace {
float calculate_radius(const glm::vec3 centre, const std::span<const Vertex> vertices)
{
    auto distance_to_centre = [&centre](const Vertex& lhs, const Vertex& rhs) {
        return glm::distance2(lhs.position, centre) < glm::distance2(rhs.position, centre);
    };
    auto farthest_vertex = std::max_element(vertices.begin(), vertices.end(), distance_to_centre);
    return distance(centre, farthest_vertex->position);
}
} // namespace

AxisAlignedBoundingBox calculate_mesh_bounding_box(const std::span<const Vertex> vertices)
{
    if (vertices.empty()) {
        return {};
    }

    glm::vec3 abb_min(std::numeric_limits<float>::max());
    glm::vec3 abb_max(std::numeric_limits<float>::min());

    for (const auto& v : vertices) {
        abb_min.x = min(v.position.x, abb_min.x);
        abb_min.y = min(v.position.y, abb_min.y);
        abb_min.z = min(v.position.z, abb_min.z);

        abb_max.x = max(v.position.x, abb_max.x);
        abb_max.y = max(v.position.y, abb_max.y);
        abb_max.z = max(v.position.z, abb_max.z);
    }

    return { abb_min, abb_max };
}

BoundingSphere calculate_mesh_bounding_sphere(const std::span<const Vertex> vertices)
{
    if (vertices.empty()) {
        return {};
    }

    const auto [min_corner, max_corner] = calculate_mesh_bounding_box(vertices);
    const glm::vec3 centre = (min_corner + max_corner) / 2.0f;
    const float radius = calculate_radius(centre, vertices);

    return { centre, radius };
}

} // namespace Mg
