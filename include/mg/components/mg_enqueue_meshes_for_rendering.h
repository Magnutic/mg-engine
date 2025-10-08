//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_enqueue_meshes_for_rendering.h
 * .
 */

#pragma once

#include "mg/components/mg_animation_component.h"
#include "mg/components/mg_mesh_component.h"
#include "mg/components/mg_transform_component.h"
#include "mg/core/ecs/mg_entity.h"
#include "mg/core/gfx/mg_render_command_list.h"
#include "mg/utils/mg_interpolate_transform.h"

namespace Mg {

inline void enqueue_meshes_for_rendering(ecs::EntityCollection& collection,
                                         gfx::RenderCommandProducer& renderlist,
                                         const float lerp_factor)
{
    for (auto [entity, transform, mesh, animation] :
         collection.get_with<TransformComponent, MeshComponent, ecs::Maybe<AnimationComponent>>()) {
        const auto interpolated =
            interpolate_transforms(transform.previous_transform, transform.transform, lerp_factor) *
            mesh.mesh_transform;

        if (animation) {
            renderlist.add_skinned_mesh(*mesh.mesh,
                                        interpolated,
                                        mesh.mesh->animation_data->skeleton,
                                        animation->pose,
                                        mesh.material_bindings);
        }
        else {
            renderlist.add_mesh(*mesh.mesh, interpolated, mesh.material_bindings);
        }
    }
}


} // namespace Mg
