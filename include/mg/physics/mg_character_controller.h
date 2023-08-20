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

#include "mg/core/mg_identifier.h"
#include "mg/physics/mg_physics.h"

namespace Mg::physics {

/** Settings for `CharacterController`. */
struct CharacterControllerSettings {
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

    /** The maximum slope angle that the character can walk up. */
    Angle max_walkable_slope = Angle::from_radians(45.0f);
};

/** CharacterController is a collision-handling physical body that can be controlled for example by
 * a player or by an AI.
 */
class CharacterController {
public:
    explicit CharacterController(Identifier id,
                                 World& world,
                                 const CharacterControllerSettings& settings);

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
    bool is_on_ground() const
    {
        return std::fabs(m_vertical_velocity) < FLT_EPSILON &&
               std::fabs(m_vertical_step) < FLT_EPSILON;
    }

    /** Get the character controller's identifier. */
    Identifier id() const { return m_id; }

    /** Get the settings with which this character controller was constructed. */
    const CharacterControllerSettings& settings() const { return m_settings; }

    /** The gravity acceleration for the character controller. */
    float gravity = 9.82f;

    /** The force with which the character pushes other objects in its way. */
    float push_force = 200.0f;

    /** Maximum fall speed, or terminal velocity, for the character. */
    float max_fall_speed = 55.0f;

    /** Mass of the character. Used for forces when colliding with dynamic objects. */
    float mass = 70.0f;

private:
    void init();

    Opt<RayHit> character_sweep_test(const glm::vec3& start,
                                     const glm::vec3& end,
                                     const glm::vec3& up,
                                     float max_surface_angle_cosine) const;
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

    // Vertical offset from current_position (capsule centre) to the character's feet.
    float feet_offset() const
    {
        // Capsule hovers step_height over the ground.
        const float capsule_height = current_height() - step_height();

        // Offset from capsule_centre to feet.
        return -(step_height() + capsule_height * 0.5f);
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

    // TODO refactor the following "ephemeral" data members to be some struct that is passed through
    // the update functions like a pipeline.
    bool m_is_standing = true;
    float m_vertical_velocity = 0.0f;
    float m_jump_velocity = 0.0f;
    float m_vertical_step = 0.0f;

    /// The desired velocity and its normalized direction, as set by the user.
    glm::vec3 m_desired_velocity = glm::vec3(0.0f);
    glm::vec3 m_desired_direction = glm::vec3(0.0f);

    glm::vec3 m_velocity_added_by_moving_surface = glm::vec3(0.0f);

    glm::vec3 m_current_position = glm::vec3(0.0f);
    glm::vec3 m_last_position = glm::vec3(0.0f);
    float m_current_step_offset = 0.0f;
    glm::vec3 m_target_position = glm::vec3(0.0f);

    float m_time_step = 1.0f; // Placeholder value to prevent division by zero before first update.

    bool m_was_on_ground = false;

    // Array of collisions. Used in recover_from_penetration but declared here to allow the
    // heap buffer to be re-used between invocations.
    std::vector<Collision> m_collisions;

    // Declared here to re-use heap buffer.
    mutable std::vector<RayHit> m_ray_hits;
};

} // namespace Mg::physics
