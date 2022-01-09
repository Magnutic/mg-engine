//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_vector_normalized.h
 * Normalized vector types: uses integral numeric types where the range of values is interpreted as
 * a fixed-point value in [0.0, 1.0].
 */

#pragma once

#include "mg/utils/mg_assert.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mg {

/** Creates a "normalized" integer of T from a floating point value in the range [-1.0f, 1.0f] (or
 * [0.0f, 1.0f], if T is unsigned) by using T's entire range as fixed-point representation. For use
 * with OpenGL's normalized vertex attributes.
 *
 * Unsafe version: assumes value is in [0.0, 1.0], if T is unsigned; or in [-1.0, 1.0], if T is
 * unsigned.
 */
template<typename T> constexpr T normalize_unsafe(float value) noexcept
{
    static_assert(std::is_integral<T>::value);
    MG_ASSERT_DEBUG(value <= 1.0f);
    MG_ASSERT_DEBUG(value >= (std::is_signed_v<T> ? -1.0f : 0.0f));
    return static_cast<T>(value * std::numeric_limits<T>::max());
}

static_assert(normalize_unsafe<uint8_t>(0.0f) == 0);
static_assert(normalize_unsafe<uint8_t>(1.0f) == 255);
static_assert(normalize_unsafe<uint8_t>(0.5f) == 127);
static_assert(normalize_unsafe<int8_t>(0.0f) == 0);
static_assert(normalize_unsafe<int8_t>(1.0f) == 127);
static_assert(normalize_unsafe<int8_t>(-1.0f) == -127);

/** Creates a "normalized" integer of T from a floating point value in the range [-1.0f, 1.0f] (or
 * [0.0f, 1.0f], if T is unsigned) by using T's entire range as fixed-point representation. For use
 * with OpenGL's normalized vertex attributes.
 *
 * For input values outside the range the fractional part is used.
 */
template<typename T> T normalize(float value) noexcept
{
    // Wrap around if value is outside normalized range.
    if (std::abs(value) > 1.0f) {
        float i;
        value = std::modf(value, &i);
    }

    return normalize_unsafe<T>(value);
}

/** Denormalize an integer, reversing the operation done by `normalize()`. */
template<typename T> constexpr float denormalize(T value) noexcept
{
    static_assert(std::is_integral<T>::value);
    return static_cast<float>(value) / std::numeric_limits<T>::max();
}

static_assert(denormalize<uint8_t>(0) == 0.0f);
static_assert(denormalize<uint8_t>(255) == 1.0f);
static_assert(denormalize<int8_t>(0) == 0.0f);
static_assert(denormalize<int8_t>(127) == 1.0f);
static_assert(denormalize<int8_t>(-127) == -1.0f);

/** Two-element normalized fixed-point vector, 16-bit elements. */
struct vec2_normalized {
    vec2_normalized() = default;
    explicit vec2_normalized(glm::vec2 vec) noexcept { set(vec); }
    explicit vec2_normalized(float x, float y) noexcept { set(glm::vec2{ x, y }); }

    void set(glm::vec2 vec) noexcept
    {
        m_x = normalize<int16_t>(vec.x);
        m_y = normalize<int16_t>(vec.y);
    }

    vec2_normalized& operator=(glm::vec2 vec) noexcept
    {
        set(vec);
        return *this;
    }

    glm::vec2 get() const noexcept { return glm::vec2(denormalize(m_x), denormalize(m_y)); }

    int16_t m_x = 0;
    int16_t m_y = 0;
};

/** Three-element normalized fixed-point vector, 16-bit elements. */
struct vec3_normalized {
    vec3_normalized() = default;
    explicit vec3_normalized(glm::vec3 vec) noexcept { set(vec); }

    explicit vec3_normalized(float x, float y, float z) noexcept { set(glm::vec3{ x, y, z }); }

    void set(glm::vec3 vec) noexcept
    {
        m_x = normalize<int16_t>(vec.x);
        m_y = normalize<int16_t>(vec.y);
        m_z = normalize<int16_t>(vec.z);
        m_w = normalize<int16_t>(0.0f);
    }

    vec3_normalized& operator=(glm::vec3 vec) noexcept
    {
        set(vec);
        return *this;
    }

    glm::vec3 get() const noexcept
    {
        return glm::vec3(denormalize(m_x), denormalize(m_y), denormalize(m_z));
    }

    int16_t m_x = 0;
    int16_t m_y = 0;
    int16_t m_z = 0;

private:
    // Padding for memory alignment
    int16_t m_w = 0;
};

/** Four-element normalized fixed-point vector, 16-bit elements. */
struct vec4_normalized {
    vec4_normalized() = default;
    explicit vec4_normalized(glm::vec4 vec) noexcept { set(vec); }

    explicit vec4_normalized(float x, float y, float z, float w) noexcept
    {
        set(glm::vec4{ x, y, z, w });
    }

    void set(glm::vec4 vec) noexcept
    {
        m_x = normalize<int16_t>(vec.x);
        m_y = normalize<int16_t>(vec.y);
        m_z = normalize<int16_t>(vec.z);
        m_w = normalize<int16_t>(vec.w);
    }

    vec4_normalized& operator=(glm::vec4 vec) noexcept
    {
        set(vec);
        return *this;
    }

    glm::vec4 get() const noexcept
    {
        return glm::vec4(denormalize(m_x), denormalize(m_y), denormalize(m_z), denormalize(m_w));
    }

    int16_t m_x = 0;
    int16_t m_y = 0;
    int16_t m_z = 0;
    int16_t m_w = 0;
};

} // namespace Mg
