//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_dynamic_body_handle.h
 * .
 */

#pragma once

#include "mg/core/physics/mg_physics_body_handle.h"
#include "mg/utils/mg_assert.h"

namespace Mg::physics {

/** Handle to a DynamicBody. Lifetime of the referee is automatically managed via reference
 * counting.
 */
class DynamicBodyHandle : public PhysicsBodyHandle {
public:
    DynamicBodyHandle() = default;

    static DynamicBodyHandle downcast(const PhysicsBodyHandle& handle)
    {
        MG_ASSERT(handle.type() == PhysicsBodyType::dynamic_body);
        return DynamicBodyHandle(handle.m_data);
    }

    glm::mat4 interpolated_transform() const;

    void set_transform(const glm::mat4& transform);

    void set_gravity(const glm::vec3& gravity);
    glm::vec3 get_gravity() const;

    //----------------------------------------------------------------------------------------------
    // Physical manipulations
    // TODO double-check that all units are correct.

    /** Apply force (in Newtons) to the DynamicBody, at the given relative position (relative to
     * centre of mass).
     */
    void apply_force(const glm::vec3& force, const glm::vec3& relative_position);

    /** Apply force (in Newtons) to the DynamicBody's centre of mass.
     * Equivalent to `apply_force(force, {0.0f, 0.0f, 0.0f})`.
     */
    void apply_central_force(const glm::vec3& force);

    /** Apply impulse (in Newton-seconds) to the DynamicBody, at the given relative position
     * (relative to centre of mass).
     */
    void apply_impulse(const glm::vec3& impulse, const glm::vec3& relative_position);

    /** Apply impulse (in Newton-seconds) to the DynamicBody's centre of mass.
     * Equivalent to `apply_impulse(impulse, {0.0f, 0.0f, 0.0f})`.
     */
    void apply_central_impulse(const glm::vec3& impulse);

    /** Apply torque (in Newton-metres) to he DynamicBody. */
    void apply_torque(const glm::vec3& torque);

    /** Apply torque impulse (in Newton-metre-seconds) to the DynamicBody. */
    void apply_torque_impulse(const glm::vec3& torque);

    void velocity(const glm::vec3& velocity);

    void angular_velocity(const glm::vec3& angular_velocity);

    void move(const glm::vec3& offset);

    /** Clear all forces and torques acting on this DynamicBody. */
    void clear_forces();

    //----------------------------------------------------------------------------------------------
    // State getters

    float mass() const;

    /** Get the linear velocity (in metres/second) for this DynamicBody. */
    glm::vec3 velocity() const;

    /** Get the angular velocity (in radians/second) for this DynamicBody. */
    glm::vec3 angular_velocity() const;

    /** Get the total force (in Newtons) acting on this DynamicBody. */
    glm::vec3 total_force() const;

    /** Get the total torque (in Newton-metres) acting on this DynamicBody. */
    glm::vec3 total_torque() const;

private:
    explicit DynamicBodyHandle(PhysicsBody* data) : PhysicsBodyHandle(data) {}

    DynamicBody& data();
    const DynamicBody& data() const;
};

/** DynamicBody objects may be Dynamic, Static, Kinematic */
enum class DynamicBodyType {
    /** Dynamic bodies are affected by gravity and collisions with other bodies. */
    Dynamic,

    /** Kinematic bodies are like static ones, but may be manually moved outside of the
     * simulation. Dynamic objects will be affected by collisions with kinematic ones, but not vice
     * versa.
     */
    Kinematic
};

/** Construction parameters for dynamic bodies. */
struct DynamicBodyParameters {
    /** What type of dynamic body it is. */
    DynamicBodyType type;

    /** The body's mass (kg). Only meaningful if `type == DynamicBodyType::Dynamic`. */
    float mass = 0.0f;

    /** Whether to enable continuous collision detection for this body. */
    bool continuous_collision_detection = false;

    /** The speed of a body, in a single physics update, must exceed the bodies radius multiplied by
     * this factor for continuous collision detection to be applied.
     */
    float continuous_collision_detection_motion_threshold = 0.0f;

    /** How much the body resists translation. */
    float linear_damping = 0.01f;

    /** How much the body resists rotation. */
    float angular_damping = 0.0f;

    /** Surface (sliding) friction. */
    float friction = 0.5f;

    /** Prevents round shapes like spheres, cylinders, and capsules from rolling forever. */
    float rolling_friction = 0.0f;

    /** Torsional friction around contact normal. */
    float spinning_friction = 0.0f;

    /** Angular factor in [0.0f, 1.0f] restricts rotations per axis. */
    glm::vec3 angular_factor = glm::vec3(1.0f, 1.0f, 1.0f);
};

} // namespace Mg::physics
