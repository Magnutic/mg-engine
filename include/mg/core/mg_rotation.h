//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_rotation.h
 * Quaternion-based rotation class. All angles are given in radians */

#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mg {

// World-space vector constants
namespace world_vector {
static const glm::vec3 forward  = glm::vec3(0.0f, 1.0f, 0.0f);
static const glm::vec3 right    = glm::vec3(1.0f, 0.0f, 0.0f);
static const glm::vec3 up       = glm::vec3(0.0f, 0.0f, 1.0f);
static const glm::vec3 backward = -forward;
static const glm::vec3 left     = -right;
static const glm::vec3 down     = -up;
} // namespace world_vector

class Rotation {
public:
    Rotation();

    /** Construct rotation from euler angles (pitch, roll, yaw) */
    explicit Rotation(glm::vec3 euler_angles);

    /** Construct Rotation from quaternion */
    explicit Rotation(glm::quat quaternion);

    glm::mat4 to_matrix() const;
    glm::quat to_quaternion() const;

    /** Returns whether rotations are similar enough to be considered
     * practically equivalent. */
    bool is_equivalent(const Rotation& rhs) const;

    /** Constructs a vector pointing forward in the orientation described by
     * this Rotation. */
    glm::vec3 forward() const;

    /** Constructs a vector pointing to the right in the orientation described
     * by this Rotation. */
    glm::vec3 right() const;

    /** Constructs a vector pointing up in the orientation described by this
     * Rotation. */
    glm::vec3 up() const;

    /** Get orientation as euler angles (pitch, roll, yaw). */
    glm::vec3 euler_angles() const { return glm::eulerAngles(m_quaternion); }

    float pitch() const { return glm::eulerAngles(m_quaternion).x; }
    float yaw() const { return glm::eulerAngles(m_quaternion).z; }
    float roll() const { return glm::eulerAngles(m_quaternion).y; }

    /** Applies angle in radians of yaw to this rotation. */
    Rotation& yaw(float angle);

    /** Applies angle in radians of pitch to this rotation. */
    Rotation& pitch(float angle);

    /** Applies angle in radians of roll to this rotation. */
    Rotation& roll(float angle);

    /** Returns the difference in angle in radians between two rotations. */
    float angle_difference(const Rotation& rhs) const;

    /** Returns the rotation created by rotating by fst then snd. */
    static Rotation combine(const Rotation& fst, const Rotation& snd);

    /** Returns the rotation needed to rotate direction vector fst to face the
     * same direction as snd. */
    static Rotation rotation_between_vectors(glm::vec3 fst, glm::vec3 snd);

    /** Returns a rotation with forward vector parallel to dir. */
    static Rotation look_to(glm::vec3 dir, glm::vec3 up = world_vector::up);

    /** Returns a rotation interpolated between two rotations by x. */
    static Rotation mix(const Rotation& from, const Rotation& to, float x);

private:
    glm::quat m_quaternion;
};

//--------------------------------------------------------------------------------------------------
// Inline implementations
//--------------------------------------------------------------------------------------------------

inline Rotation::Rotation() : m_quaternion{ glm::vec3{ 0.0f, 0.0f, 0.0f } } {}

inline Rotation::Rotation(glm::vec3 euler_angles) : m_quaternion{ euler_angles } {}

inline Rotation::Rotation(glm::quat quaternion) : m_quaternion{ quaternion } {}

inline glm::mat4 Rotation::to_matrix() const
{
    return glm::toMat4(m_quaternion);
}

inline glm::quat Rotation::to_quaternion() const
{
    return m_quaternion;
}

inline bool Rotation::is_equivalent(const Rotation& rhs) const
{
    float matching = glm::dot(m_quaternion, rhs.m_quaternion);
    return (glm::abs(matching - 1.0f) < 0.001f);
}

inline Rotation Rotation::combine(const Rotation& fst, const Rotation& snd)
{
    return Rotation{ snd.m_quaternion * fst.m_quaternion };
}

inline float Rotation::angle_difference(const Rotation& rhs) const
{
    return glm::acos(dot(forward(), rhs.forward()));
}

inline Rotation Rotation::mix(const Rotation& from, const Rotation& to, float x)
{
    return Rotation{ glm::slerp(from.m_quaternion, to.m_quaternion, x) };
}

inline glm::vec3 Rotation::forward() const
{
    return m_quaternion * world_vector::forward;
}

inline glm::vec3 Rotation::right() const
{
    return m_quaternion * world_vector::right;
}

inline glm::vec3 Rotation::up() const
{
    return m_quaternion * world_vector::up;
}

inline Rotation& Rotation::yaw(float angle)
{
    m_quaternion *= glm::angleAxis(angle, world_vector::up);
    return *this;
}

inline Rotation& Rotation::pitch(float angle)
{
    m_quaternion *= glm::angleAxis(angle, world_vector::right);
    return *this;
}

inline Rotation& Rotation::roll(float angle)
{
    m_quaternion *= glm::angleAxis(angle, world_vector::forward);
    return *this;
}

} // namespace Mg
