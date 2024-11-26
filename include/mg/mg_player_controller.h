//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_player_controller.h
 * Takes input to control player character controller.
 */

#pragma once

#include "mg/core/mg_rotation.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_macros.h"

#include <memory>

namespace Mg::input {
class Keyboard;
class Mouse;
class ButtonTracker;
class MouseMovementTracker;
} // namespace Mg::input

namespace Mg::physics {
class CharacterController;
}

namespace Mg {

class IPlayerController {
public:
    MG_INTERFACE_BOILERPLATE(IPlayerController);

    /** Read user input and apply movements to the character controller accordingly.
     * Do this before updating the physics world.
     */
    virtual void handle_movement_inputs(physics::CharacterController& character_controller) = 0;

    /** Read user input and update rotation accordingly. This is separated from the main update
     * function, because we want to call it more often: if we update the rotation once for every
     * rendered frame, instead of every logical time step, we get less visible input lag when we use
     * the rotation to orient the scene camera.
     */
    virtual void handle_rotation_inputs(float sensitivity_x, float sensitivity_y) = 0;

    virtual Rotation get_rotation() const = 0;

    virtual void set_rotation(Rotation) = 0;
};

class PlayerController : public IPlayerController {
public:
    MG_MAKE_NON_MOVABLE(PlayerController);
    MG_MAKE_NON_COPYABLE(PlayerController);

    explicit PlayerController(std::shared_ptr<input::ButtonTracker> button_tracker,
                              std::shared_ptr<input::MouseMovementTracker> mouse_movement_tracker);

    ~PlayerController() override;

    void handle_movement_inputs(physics::CharacterController& character_controller) override;

    void handle_rotation_inputs(float sensitivity_x, float sensitivity_y) override;

    Rotation get_rotation() const override { return rotation; }

    void set_rotation(Rotation rotation_) override { rotation = rotation_; }

    Rotation rotation;
    float acceleration = 0.6f;
    float max_horizontal_speed = 10.0f;
    float friction = 0.6f;

private:
    std::shared_ptr<input::ButtonTracker> m_button_tracker;
    std::shared_ptr<input::MouseMovementTracker> m_mouse_movement_tracker;
};

class EditorController : public IPlayerController {
public:
    MG_MAKE_NON_MOVABLE(EditorController);
    MG_MAKE_NON_COPYABLE(EditorController);

    explicit EditorController(std::shared_ptr<input::ButtonTracker> button_tracker,
                              std::shared_ptr<input::MouseMovementTracker> mouse_movement_tracker);

    ~EditorController() override;

    void handle_movement_inputs(physics::CharacterController& character_controller) override;

    void handle_rotation_inputs(float sensitivity_x, float sensitivity_y) override;

    Rotation get_rotation() const override { return rotation; }

    void set_rotation(Rotation rotation_) override { rotation = rotation_; }

    Rotation rotation;
    float acceleration = 0.6f;
    float max_speed = 10.0f;
    float max_speed_fast = 20.0f;
    float friction = 0.6f;

private:
    std::shared_ptr<input::ButtonTracker> m_button_tracker;
    std::shared_ptr<input::MouseMovementTracker> m_mouse_movement_tracker;
};

} // namespace Mg
