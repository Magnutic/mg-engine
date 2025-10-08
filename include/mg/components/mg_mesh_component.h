//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_component.h
 * .
 */

#pragma once

#include "mg/core/ecs/mg_base_component.h"
#include "mg/core/gfx/mg_material_binding.h"
#include "mg/core/gfx/mg_mesh.h"

#include <glm/mat4x4.hpp>

namespace Mg {

struct MeshComponent : ecs::BaseComponent<MeshComponent> {
    const gfx::Mesh* mesh = nullptr;
    small_vector<gfx::MaterialBinding, 4> material_bindings;
    glm::mat4 mesh_transform = glm::mat4(1.0f);
};


} // namespace Mg
