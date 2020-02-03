//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_rotation.h
 * Quaternion-based rotation class. All angles are given in radians.
 */

#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#    define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mg {

// World-space vector constants
namespace world_vector {
static const glm::vec3 forward = glm::vec3(0.0f, 1.0f, 0.0f);
static const glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
static const glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
static const glm::vec3 backward = -forward;
static const glm::vec3 left = -right;
static const glm::vec3 down = -up;
} // namespace world_vector

class Rotation {
public:
    Rotation() noexcept;

    /** Construct rotation from euler angles (pitch, roll, yaw) */
    explicit Rotation(glm::vec3 euler_angles) noexcept;

    /** Construct Rotation from quaternion */
    explicit Rotation(glm::quat quaternion) noexcept;

    glm::mat4 to_matrix() const noexcept;
    glm::quat to_quaternion() const noexcept;

    /** Returns whether rotations are similar enough to be considered
     * practically equivalent. */
    bool is_equivalent(const Rotation& rhs) const noexcept;

    /** Constructs a vector pointing forward in the orientation described by
     * this Rotation. */
    glm::vec3 forward() const noexcept;

    /** Constructs a vector pointing to the right in the orientation described
     * by this Rotation. */
    glm::vec3 right() const noexcept;

    /** Constructs a vector pointing up in the orientation described by this
     * Rotation. */
    glm::vec3 up() const noexcept;

    /** Get orientation as euler angles (pitch, roll, yaw). */
    glm::vec3 euler_angles() const noexcept { return glm::eulerAngles(m_quaternion); }

    float pitch() const noexcept { return glm::eulerAngles(m_quaternion).x; }
    float yaw() const noexcept { return glm::eulerAngles(m_quaternion).z; }
    float roll() const noexcept { return glm::eulerAngles(m_quaternion).y; }

    /** Applies angle in radians of yaw to this rotation. */
    Rotation& yaw(float angle) noexcept;

    /** Applies angle in radians of pitch to this rotation. */
    Rotation& pitch(float angle) noexcept;

    /** Applies angle in radians of roll to this rotation. */
    Rotation& roll(float angle) noexcept;

    /** Returns the difference in angle in radians between two rotations. */
    float angle_difference(const Rotation& rhs) const noexcept;

    /** Returns the rotation created by rotating by fst then snd. */
    static Rotation combine(const Rotation& fst, const Rotation& snd) noexcept;

    /** Returns the rotation needed to rotate direction vector fst to face the
     * same direction as snd. */
    static Rotation rotation_between_vectors(glm::vec3 fst, glm::vec3 snd) noexcept;

    /** Returns a rotation with forward vector parallel to dir. */
    static Rotation look_to(glm::vec3 dir, glm::vec3 up = world_vector::up) noexcept;

    /** Returns a rotation interpolated between two rotations by x. */
    static Rotation mix(const Rotation& from, const Rotation& to, float x) noexcept;

private:
    glm::quat m_quaternion;
};

//--------------------------------------------------------------------------------------------------
// Inline implementations
//--------------------------------------------------------------------------------------------------

inline Rotation::Rotation() noexcept : m_quaternion{ glm::vec3{ 0.0f, 0.0f, 0.0f } } {}

inline Rotation::Rotation(glm::vec3 euler_angles) noexcept : m_quaternion{ euler_angles } {}

inline Rotation::Rotation(glm::quat quaternion) noexcept : m_quaternion{ quaternion } {}

inline glm::mat4 Rotation::to_matrix() const noexcept
{
    return glm::toMat4(m_quaternion);
}

inline glm::quat Rotation::to_quaternion() const noexcept
{
    return m_quaternion;
}

inline bool Rotation::is_equivalent(const Rotation& rhs) const noexcept
{
    const float matching = glm::dot(m_quaternion, rhs.m_quaternion);
    return (glm::abs(matching - 1.0f) < 0.001f);
}

inline Rotation Rotation::combine(const Rotation& fst, const Rotation& snd) noexcept
{
    return Rotation{ snd.m_quaternion * fst.m_quaternion };
}

inline float Rotation::angle_difference(const Rotation& rhs) const noexcept
{
    return glm::acos(dot(forward(), rhs.forward()));
}

inline Rotation Rotation::mix(const Rotation& from, const Rotation& to, float x) noexcept
{
    return Rotation{ glm::slerp(from.m_quaternion, to.m_quaternion, x) };
}

inline glm::vec3 Rotation::forward() const noexcept
{
    return m_quaternion * world_vector::forward;
}

inline glm::vec3 Rotation::right() const noexcept
{
    return m_quaternion * world_vector::right;
}

inline glm::vec3 Rotation::up() const noexcept
{
    return m_quaternion * world_vector::up;
}

inline Rotation& Rotation::yaw(float angle) noexcept
{
    m_quaternion *= glm::angleAxis(angle, world_vector::up);
    return *this;
}

inline Rotation& Rotation::pitch(float angle) noexcept
{
    m_quaternion *= glm::angleAxis(angle, world_vector::right);
    return *this;
}

inline Rotation& Rotation::roll(float angle) noexcept
{
    m_quaternion *= glm::angleAxis(angle, world_vector::forward);
    return *this;
}

} // namespace Mg
