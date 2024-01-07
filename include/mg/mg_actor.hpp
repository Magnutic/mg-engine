//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_actor.h
 * Game actor that can move around in the physics world.
 */

#pragma once

#include "mg/physics/mg_character_controller.h"

namespace Mg {

class Actor {
public:
    constexpr static float radius = 0.5f;
    constexpr static float height = 1.8f;
    constexpr static float step_height = 0.6f;

    explicit Actor(Identifier id,
                   Mg::physics::World& physics_world,
                   const glm::vec3& position,
                   const float mass)
        : character_controller(id, physics_world, {})
    {
        character_controller.mass = mass;
        character_controller.set_position(position);
    }

    void update(glm::vec3 acceleration, float jump_impulse);

    glm::vec3 position(const float interpolate = 1.0f) const
    {
        return character_controller.get_position(interpolate);
    }

    float current_height() const { return character_controller.current_height(); }

    Mg::physics::CharacterController character_controller;
    float max_horizontal_speed = 10.0f;
    float friction = 0.6f;
};

} // namespace Mg
