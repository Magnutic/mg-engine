//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

// This character controller was initially based on the character controller code bundled with the
// Bullet physics library. It has since been heavily modified.
//
// Original license:
//
//    Bullet Continuous Collision Detection and Physics Library Copyright (c) 2003-2008 Erwin
//    Coumans http://bulletphysics.com
//
//    This software is provided 'as-is', without any express or implied warranty.  In no event will
//    the authors be held liable for any damages arising from the use of this software.  Permission
//    is granted to anyone to use this software for any purpose, including commercial applications,
//    and to alter it and redistribute it freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not claim that you wrote
//    the original software. If you use this software in a product, an acknowledgment in the product
//    documentation would be appreciated but is not required.
//    2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//    being the original software.
//    3. This notice may not be removed or altered from any source distribution.

/** @file mg_character_controller.h
 * Collision-handling physical body that can be controlled for example by a player or by an AI.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/physics/mg_physics.h"

namespace Mg::physics {

struct ImmutableCharacterControllerSettings {
    /** Radius of the character's collision body. */
    float radius = 0.5f;

    /** Total height of the character when standing straight. */
    float standing_height = 1.8f;

    /** Total height of the character when crouching. */
    float crouching_height = 0.6f;

    /** The maximum height difference that the character may step over when standing. */
    float standing_step_height = 0.6f;

    /** The maximum height difference that the character may step over when crouching. */
    float crouching_step_height = 0.3f;
};

struct MutableCharacterControllerSettings {
    /** The maximum slope angle that the character can walk up. */
    Angle max_walkable_slope = Angle::from_degrees(45.0f);

    /** Horizontal acceleration applied when sliding down a slope. */
    float slide_down_acceleration = 0.5f;

    /** The gravity acceleration for the character controller. */
    float gravity = 9.82f;

    /** The force with which the character pushes other objects in its way. */
    float push_force = 200.0f;

    /** Maximum fall speed, or terminal velocity, for the character. */
    float max_fall_speed = 55.0f;

    /** Mass of the character. Used for forces when colliding with dynamic objects. */
    float mass = 70.0f;

    /** When moving up and down stairs, smooth the vertical movement by applying the motion by this
     * multiplied by this factor each step.
     */
    float vertical_interpolation_factor = 0.35f;
};

/** Settings for `CharacterController`. */
struct CharacterControllerSettings : ImmutableCharacterControllerSettings,
                                     MutableCharacterControllerSettings {};

/** CharacterController is a collision-handling physical body that can be controlled for example by
 * a player or by an AI.
 */
class CharacterController {
public:
    explicit CharacterController(Identifier id,
                                 World& world,
                                 const CharacterControllerSettings& settings,
                                 const glm::vec3& initial_position = glm::vec3(0.0f));

    ~CharacterController() = default;

    MG_MAKE_NON_COPYABLE(CharacterController);
    MG_MAKE_NON_MOVABLE(CharacterController);

    void update(float time_step);

    /** Get the current position of the character controller's "feet".
     * @param interpolate Factor for interpolating between last update's position and the most
     * recent position. When using a fixed update time step but variable framerate, this can be used
     * to prevent choppy motion. The default value of 1.0f will always return the most recent
     * position.
     */
    glm::vec3 get_position(float interpolate = 1.0f) const;

    /** Directly set position of character controller's "feet", ignoring collisions. For
     * regular movement, use `move` instead. To also clear motion state, call `reset`.
     */
    void set_position(const glm::vec3& position);


    /** Moves the character with the given velocity. */
    void move(const glm::vec3& velocity);

    /** Jump by setting the vertical velocity to the given velocity. Note that this will apply the
     * vertical velocity whether or not the character controller is on the ground. To prevent
     * jumping mid-air, check `is_on_ground()` first.
     */
    void jump(float velocity);

    /** Ignore gravity during the next update. */
    void ignore_gravity() { m_ignore_gravity = true; }

    /** Clear all motion state. */
    void reset();

    /** Set whether the character is standing or crouching. Returns whether the state changed.
     * Reasons why it might not change: 1. It was already in the requested state. 2. The character
     * could not stand because something blocked above.
     */
    bool set_is_standing(bool v);

    /** Get whether the character is standing (as opposed to crouching). */
    bool get_is_standing() const { return m_is_standing; }

    /** Current height, taking into account whether the character is standing or crouching. */
    float current_height() const
    {
        return m_is_standing ? m_settings.standing_height : m_settings.crouching_height;
    }

    /** Like current_height, but with smooth transitions between crouching and standing states.
     * @param interpolate Factor for interpolating between last update's height and the most
     * recent height. When using a fixed update time step but variable framerate, this can be used
     * to prevent choppy motion. The default value of 1.0f will always return the most recent
     * position.
     */
    float current_height_smooth(const float interpolate = 1.0f) const
    {
        return glm::mix(m_last_height_interpolated, m_current_height_interpolated, interpolate);
    }


    /** Get the character's velocity (in m/s) from the most recent update. */
    glm::vec3 velocity() const;

    /** Get the velocity that was added to the character in the most recent update, due to
     * movement of the surface on which the character stands.
     * This can be used to remove or reduce the inertia from such movements. Otherwise, the
     * character is likely to helplessly fall off moving objects when they change direction or
     * speed.
     */
    glm::vec3 velocity_added_by_moving_surface() const
    {
        return m_velocity_added_by_moving_surface;
    }

    /** Get whether the character controller is standing on the ground (as opposed to being in air).
     */
    bool is_on_ground() const { return m_is_on_ground; }

    /** Get the character controller's identifier. */
    Identifier id() const { return m_id; }

    /** Get the settings with which this character controller was constructed. */
    const CharacterControllerSettings& settings() const { return m_settings; }

    /** Get those settings which can be changed even after the controller is constructed. */
    MutableCharacterControllerSettings& mutable_settings() { return m_settings; }

private:
    void init(const glm::vec3& initial_position);

    Opt<RayHit> character_sweep_test(const glm::vec3& start,
                                     const glm::vec3& end,
                                     const glm::vec3& up,
                                     float min_normal_angle_cosine = -1.0f,
                                     float max_normal_angle_cosine = 1.0f) const;
    void recover_from_penetration();
    void step_up();
    void horizontal_step(const glm::vec3& step);
    void step_down();

    Shape* shape() const { return m_is_standing ? m_standing_shape : m_crouching_shape; }

    GhostObjectHandle& collision_body()
    {
        return m_is_standing ? m_standing_collision_body : m_crouching_collision_body;
    }
    const GhostObjectHandle& collision_body() const
    {
        return m_is_standing ? m_standing_collision_body : m_crouching_collision_body;
    }

    float step_height() const
    {
        return m_is_standing ? m_settings.standing_step_height : m_settings.crouching_step_height;
    }

    // Vertical offset from current_position (collision-body centre) to the character's feet.
    float feet_offset() const
    {
        // Body hovers step_height over the ground.
        const float body_height = current_height() - step_height();

        // Offset from collision-body centre to feet.
        return -(step_height() + body_height * 0.5f);
    }

    // Settings for the character controller.
    CharacterControllerSettings m_settings;

    Identifier m_id;

    World* m_world = nullptr;

    GhostObjectHandle m_standing_collision_body;
    GhostObjectHandle m_crouching_collision_body;
    Shape* m_standing_shape = nullptr;
    Shape* m_crouching_shape = nullptr;
    float m_max_slope_radians = 0.0f;
    float m_max_slope_cosine = 0.0f;

    float m_time_step = 1.0f;
    float m_vertical_velocity = 0.0f;
    float m_jump_velocity = 0.0f;
    float m_last_height_interpolated = 0.0f;
    float m_current_height_interpolated = 0.0f;
    bool m_is_standing = true;
    bool m_is_on_ground = false;
    bool m_ignore_gravity = false;

    /// The desired velocity and its normalized direction, as set by the user.
    glm::vec3 m_desired_velocity = glm::vec3(0.0f);
    glm::vec3 m_desired_direction = glm::vec3(0.0f);

    glm::vec3 m_velocity_added_by_moving_surface = glm::vec3(0.0f);

    glm::vec3 m_current_position = glm::vec3(0.0f);
    glm::vec3 m_last_position = glm::vec3(0.0f);

    // Array of collisions. Used in recover_from_penetration but declared here to allow the
    // heap buffer to be re-used between invocations.
    std::vector<Collision> m_collisions;

    // Declared here to re-use heap buffer.
    mutable std::vector<RayHit> m_ray_hits;
};

} // namespace Mg::physics
