//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/mg_player_controller.h"

#include "mg/input/mg_input.h"
#include "mg/physics/mg_character_controller.h"

namespace Mg {

namespace {

Rotation handle_rotation_inputs_impl(const Rotation& rotation_before,
                                     input::MouseMovementTracker& mouse_movement_tracker,
                                     const float sensitivity_x,
                                     const float sensitivity_y)
{
    static constexpr float max_sensitivity_factor = 0.01f;
    const auto delta = mouse_movement_tracker.mouse_delta();
    Angle pitch = rotation_before.pitch() -
                  Angle::from_radians(delta.y * max_sensitivity_factor * sensitivity_x);
    Angle yaw = rotation_before.yaw() -
                Angle::from_radians(delta.x * max_sensitivity_factor * sensitivity_y);

    pitch = Angle::clamp(pitch, -90_degrees, 90_degrees);

    // Set rotation from euler angles, clamping pitch between straight down & straight up.
    return Rotation{ glm::vec3{ pitch.radians(), 0.0f, yaw.radians() } };
}

} // namespace

PlayerController::PlayerController(
    std::shared_ptr<input::ButtonTracker> button_tracker,
    std::shared_ptr<input::MouseMovementTracker> mouse_movement_tracker)
    : m_button_tracker{ std::move(button_tracker) }
    , m_mouse_movement_tracker{ std::move(mouse_movement_tracker) }
{
    MG_ASSERT(m_button_tracker && m_mouse_movement_tracker);

    m_button_tracker->bind("forward", input::Key::W);
    m_button_tracker->bind("backward", input::Key::S);
    m_button_tracker->bind("left", input::Key::A);
    m_button_tracker->bind("right", input::Key::D);
    m_button_tracker->bind("jump", input::Key::Space);
    m_button_tracker->bind("crouch", input::Key::LeftControl);
}

PlayerController::~PlayerController() = default;

void PlayerController::handle_movement_inputs(physics::CharacterController& character_controller)
{
    auto button_states = m_button_tracker->get_button_events();

    auto get_is_held = [&](Identifier mapping) -> float {
        return static_cast<float>(button_states[mapping].is_held);
    };

    const bool is_jumping = button_states["jump"].is_held && character_controller.is_on_ground();
    const auto jump_impulse = (is_jumping ? 5.0f : 0.0f) *
                              (character_controller.get_is_standing() ? 1.0f : 0.5f);

    auto forward_acc = acceleration * (get_is_held("forward") - get_is_held("backward"));
    auto right_acc = acceleration * (get_is_held("right") - get_is_held("left"));

    const Rotation rotation_horizontal(glm::vec3(0.0f, 0.0f, rotation.euler_angles().z));
    const glm::vec3 vec_forward = rotation_horizontal.forward();
    const glm::vec3 vec_right = rotation_horizontal.right();

    // Move slower when crouching or in mid-air.
    const float acceleration_factor = (character_controller.is_on_ground() ? 1.0f : 0.3f) *
                                      (character_controller.get_is_standing() ? 1.0f : 0.5f);

    const auto max_speed = max_horizontal_speed * (character_controller.get_is_standing() ||
                                                           !character_controller.is_on_ground()
                                                       ? 1.0f
                                                       : 0.5f);

    const float conditional_friction = character_controller.is_on_ground() ? friction : 0.0f;

    const auto acceleration_vector = (vec_forward * forward_acc + vec_right * right_acc) *
                                     acceleration_factor;

    // Keep old velocity for inertia, but discard velocity added by platform movement, as that makes
    // the actor far too prone to uncontrollably sliding off surfaces.
    auto horizontal_velocity = character_controller.velocity() -
                               character_controller.velocity_added_by_moving_surface();
    horizontal_velocity.x += acceleration_vector.x;
    horizontal_velocity.y += acceleration_vector.y;
    horizontal_velocity.z = 0.0f;

    // Compensate for friction.
    if (glm::length2(acceleration_vector) > 0.0f) {
        horizontal_velocity += glm::normalize(acceleration_vector) * conditional_friction;
    }

    // Apply friction.
    if (glm::length2(horizontal_velocity) >= 0.0f) {
        if (glm::length2(horizontal_velocity) <= conditional_friction * conditional_friction) {
            horizontal_velocity = glm::vec3(0.0f);
        }
        else {
            horizontal_velocity -= glm::normalize(horizontal_velocity) * conditional_friction;
        }
    }

    // Limit horizontal_speed.
    const float horizontal_speed = glm::length(horizontal_velocity);
    if (horizontal_speed > max_speed) {
        horizontal_velocity.x *= max_speed / horizontal_speed;
        horizontal_velocity.y *= max_speed / horizontal_speed;
    }


    // Apply movements to character controller.
    character_controller.set_is_standing(!button_states["crouch"].is_held);
    character_controller.move(horizontal_velocity);
    if (jump_impulse > 0.0f) {
        character_controller.jump(jump_impulse);
    }
}

void PlayerController::handle_rotation_inputs(const float sensitivity_x, const float sensitivity_y)
{
    rotation = handle_rotation_inputs_impl(rotation,
                                           *m_mouse_movement_tracker,
                                           sensitivity_x,
                                           sensitivity_y);
}

EditorController::EditorController(
    std::shared_ptr<input::ButtonTracker> button_tracker,
    std::shared_ptr<input::MouseMovementTracker> mouse_movement_tracker)
    : m_button_tracker{ std::move(button_tracker) }
    , m_mouse_movement_tracker{ std::move(mouse_movement_tracker) }
{
    MG_ASSERT(m_button_tracker && m_mouse_movement_tracker);

    m_button_tracker->bind("forward", input::Key::W);
    m_button_tracker->bind("backward", input::Key::S);
    m_button_tracker->bind("left", input::Key::A);
    m_button_tracker->bind("right", input::Key::D);
    m_button_tracker->bind("float_up", input::Key::Space);
    m_button_tracker->bind("float_down", input::Key::LeftControl);
    m_button_tracker->bind("fast_switch", input::Key::LeftShift);
}

EditorController::~EditorController() = default;

void EditorController::handle_movement_inputs(physics::CharacterController& character_controller)
{
    auto button_states = m_button_tracker->get_button_events();

    auto get_is_held = [&](Identifier mapping) -> float {
        return static_cast<float>(button_states[mapping].is_held);
    };

    const auto up_acc = acceleration * (get_is_held("float_up") - get_is_held("float_down"));
    const auto forward_acc = acceleration * (get_is_held("forward") - get_is_held("backward"));
    const auto right_acc = acceleration * (get_is_held("right") - get_is_held("left"));

    const auto vec_forward = rotation.forward();
    const auto vec_right = rotation.right();
    const auto acceleration_vector = world_vector::up * up_acc + vec_forward * forward_acc +
                                     vec_right * right_acc;

    auto velocity = character_controller.velocity() + acceleration_vector;

    // Compensate for friction.
    if (glm::length2(acceleration_vector) > 0.0f) {
        velocity += glm::normalize(acceleration_vector) * friction;
    }

    // Apply friction.
    if (glm::length2(velocity) >= 0.0f) {
        if (glm::length2(velocity) <= friction * friction) {
            velocity = glm::vec3(0.0f);
        }
        else {
            velocity -= glm::normalize(velocity) * friction;
        }
    }

    // Limit horizontal_speed.
    const auto speed_limit = button_states["fast_switch"].is_held ? max_speed_fast : max_speed;
    const auto speed = glm::length(velocity);
    if (speed > speed_limit) {
        velocity *= speed_limit / speed;
    }


    // Apply movements to character controller.
    character_controller.ignore_gravity();
    character_controller.set_is_standing(true);
    character_controller.move(velocity);
}

void EditorController::handle_rotation_inputs(const float sensitivity_x, const float sensitivity_y)
{
    rotation = handle_rotation_inputs_impl(rotation,
                                           *m_mouse_movement_tracker,
                                           sensitivity_x,
                                           sensitivity_y);
}

} // namespace Mg
