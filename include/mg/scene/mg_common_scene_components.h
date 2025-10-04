//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_common_scene_components.h
 * ECS Components usable in most scenes.
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/ecs/mg_component.h"
#include "mg/ecs/mg_entity.h"
#include "mg/gfx/mg_mesh.h"
#include "mg/gfx/mg_render_command_list.h"
#include "mg/gfx/mg_skeleton.h"
#include "mg/physics/mg_dynamic_body_handle.h"
#include "mg/physics/mg_static_body_handle.h"
#include "mg/utils/mg_interpolate_transform.h"

#include <glm/mat4x4.hpp>

namespace Mg::scene {

struct TransformComponent : ecs::BaseComponent<TransformComponent> {
    glm::mat4 previous_transform = glm::mat4(1.0f);
    glm::mat4 transform = glm::mat4(1.0f);
};

struct StaticBodyComponent : ecs::BaseComponent<StaticBodyComponent> {
    physics::StaticBodyHandle physics_body;
};

struct DynamicBodyComponent : ecs::BaseComponent<DynamicBodyComponent> {
    physics::DynamicBodyHandle physics_body;
};

struct MeshComponent : ecs::BaseComponent<MeshComponent> {
    const gfx::Mesh* mesh = nullptr;
    small_vector<gfx::MaterialBinding, 4> material_bindings;
    glm::mat4 mesh_transform = glm::mat4(1.0f);
};

struct AnimationComponent : ecs::BaseComponent<AnimationComponent> {
    Opt<uint32_t> current_clip;
    float time_in_clip = 0.0f;
    float animation_speed = 1.0f;
    gfx::SkeletonPose pose;
};

inline void update_dynamic_body_transforms(ecs::EntityCollection& collection)
{
    for (auto [entity, dynamic_body, transform] :
         collection.get_with<DynamicBodyComponent, TransformComponent>()) {
        transform.previous_transform = transform.transform;
        transform.transform = dynamic_body.physics_body.get_transform();
    }
}

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

inline void add_meshes_to_render_list(ecs::EntityCollection& collection,
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

} // namespace Mg::scene
