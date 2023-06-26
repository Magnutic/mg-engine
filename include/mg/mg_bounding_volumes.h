//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_bounding_primitives.h
 * Type definitions and functions for calculating bounding primitives.
 */

#pragma once

#include "mg/utils/mg_gsl.h"

#include <glm/vec3.hpp>

namespace Mg::gfx::Mesh {
struct Vertex;
}

namespace Mg {

struct BoundingSphere {
    glm::vec3 centre{ 0.0f };
    float radius = 0.0f;
};

struct AxisAlignedBoundingBox {
    glm::vec3 min_corner{ 0.0f };
    glm::vec3 max_corner{ 0.0f };
};

AxisAlignedBoundingBox calculate_mesh_bounding_box(std::span<const gfx::Mesh::Vertex> vertices);

BoundingSphere calculate_mesh_bounding_sphere(std::span<const gfx::Mesh::Vertex> vertices);

} // namespace Mg
