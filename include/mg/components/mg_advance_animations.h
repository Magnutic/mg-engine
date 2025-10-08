//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_advance_animations.h
 * .
 */

#pragma once

#include "mg/core/ecs/mg_entity.h"
#include "mg/components/mg_animation_component.h"
#include "mg/components/mg_mesh_component.h"

namespace Mg {

inline void advance_animations(ecs::EntityCollection& collection, const float delta_time)
{
    for (auto [entity, mesh, animation] :
         collection.get_with<MeshComponent, AnimationComponent>()) {
        if (!animation.current_clip.has_value()) {
            animation.pose = mesh.mesh->animation_data->skeleton.get_bind_pose();
            continue;
        }

        animation.time_in_clip = delta_time * animation.animation_speed;

        const auto clip_index = animation.current_clip.value();
        gfx::animate_skeleton(mesh.mesh->animation_data->clips[clip_index],
                              animation.pose,
                              animation.time_in_clip);
    }
}


} // namespace Mg
