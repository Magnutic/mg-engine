//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_player_controller.h
 * Takes input to control player character controller.
 */

#pragma once

#include "mg/physics/mg_character_controller.h"
#include "mg/utils/mg_assert.h"

#include <memory>

namespace Mg::input {
class Keyboard;
class Mouse;
class ButtonTracker;
class MouseMovementTracker;
} // namespace Mg::input

namespace Mg {

class PlayerController {
public:
    MG_MAKE_NON_MOVABLE(PlayerController);
    MG_MAKE_NON_COPYABLE(PlayerController);

    explicit PlayerController(Identifier id,
                              physics::World& world,
                              const physics::CharacterControllerSettings& settings,
                              std::shared_ptr<input::ButtonTracker> button_tracker,
                              std::shared_ptr<input::MouseMovementTracker> mouse_movement_tracker);

    ~PlayerController();

    /** Read user input and apply movements to the character controller accordingly.
     * Do this before updating the physics world.
     */
    void handle_movement_inputs();

    /** Read user input and update rotation accordingly. This is separated from the main update
     * function, because we want to call it more often: if we update the rotation once for every
     * rendered frame, instead of every logical time step, we get less visible input lag when we use
     * the rotation to orient the scene camera.
     */
    void handle_rotation_inputs(float sensitivity_x, float sensitivity_y);

    /** Update the character controller. Do this after updating the physics world. */
    void update(const float time_step)
    {
        // TODO remove this; physics world shall instead automatically update all character
        // controllers in it just as it updates all other bodies.
        character_controller.update(time_step);
    }

    physics::CharacterController character_controller;
    Rotation rotation;
    float acceleration = 0.6f;
    float max_horizontal_speed = 10.0f;
    float friction = 0.6f;

private:
    std::shared_ptr<input::ButtonTracker> m_button_tracker;
    std::shared_ptr<input::MouseMovementTracker> m_mouse_movement_tracker;
};

} // namespace Mg
