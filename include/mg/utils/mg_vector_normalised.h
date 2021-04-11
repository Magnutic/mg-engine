//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_vector_normalised.h
 * Normalised vector types: uses integral numeric types where the range of values is interpreted as
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

/** Creates a "normalised" integer of T from a floating point value in the range [-1.0f, 1.0f] (or
 * [0.0f, 1.0f], if T is unsigned) by using T's entire range as fixed-point representation. For use
 * with OpenGL's normalised vertex attributes.
 *
 * Unsafe version: assumes value is in [0.0, 1.0], if T is unsigned; or in [-1.0, 1.0], if T is
 * unsigned.
 */
template<typename T> constexpr T normalise_unsafe(float value) noexcept
{
    static_assert(std::is_integral<T>::value);
    MG_ASSERT_DEBUG(value <= 1.0f);
    MG_ASSERT_DEBUG(value >= (std::is_signed_v<T> ? -1.0f : 0.0f));
    return static_cast<T>(value * std::numeric_limits<T>::max());
}

static_assert(normalise_unsafe<uint8_t>(0.0f) == 0);
static_assert(normalise_unsafe<uint8_t>(1.0f) == 255);
static_assert(normalise_unsafe<uint8_t>(0.5f) == 127);
static_assert(normalise_unsafe<int8_t>(0.0f) == 0);
static_assert(normalise_unsafe<int8_t>(1.0f) == 127);
static_assert(normalise_unsafe<int8_t>(-1.0f) == -127);

/** Creates a "normalised" integer of T from a floating point value in the range [-1.0f, 1.0f] (or
 * [0.0f, 1.0f], if T is unsigned) by using T's entire range as fixed-point representation. For use
 * with OpenGL's normalised vertex attributes.
 *
 * For input values outside the range the fractional part is used.
 */
template<typename T> T normalise(float value) noexcept
{
    // Wrap around if value is outside normalised range.
    if (std::abs(value) > 1.0f) {
        float i;
        value = std::modf(value, &i);
    }

    return normalise_unsafe<T>(value);
}

/** Denormalise an integer, reversing the operation done by `normalise()`. */
template<typename T> constexpr float denormalise(T value) noexcept
{
    static_assert(std::is_integral<T>::value);
    return static_cast<float>(value) / std::numeric_limits<T>::max();
}

static_assert(denormalise<uint8_t>(0) == 0.0f);
static_assert(denormalise<uint8_t>(255) == 1.0f);
static_assert(denormalise<int8_t>(0) == 0.0f);
static_assert(denormalise<int8_t>(127) == 1.0f);
static_assert(denormalise<int8_t>(-127) == -1.0f);

/** Two-element normalised fixed-point vector, 16-bit elements. */
struct vec2_normalised {
    vec2_normalised() = default;
    explicit vec2_normalised(glm::vec2 vec) noexcept { set(vec); }
    explicit vec2_normalised(float x, float y) noexcept { set(glm::vec2{ x, y }); }

    void set(glm::vec2 vec) noexcept
    {
        m_x = normalise<int16_t>(vec.x);
        m_y = normalise<int16_t>(vec.y);
    }

    vec2_normalised& operator=(glm::vec2 vec) noexcept
    {
        set(vec);
        return *this;
    }

    glm::vec2 get() const noexcept { return glm::vec2(denormalise(m_x), denormalise(m_y)); }

    int16_t m_x = 0;
    int16_t m_y = 0;
};

/** Three-element normalised fixed-point vector, 16-bit elements. */
struct vec3_normalised {
    vec3_normalised() = default;
    explicit vec3_normalised(glm::vec3 vec) noexcept { set(vec); }

    explicit vec3_normalised(float x, float y, float z) noexcept { set(glm::vec3{ x, y, z }); }

    void set(glm::vec3 vec) noexcept
    {
        m_x = normalise<int16_t>(vec.x);
        m_y = normalise<int16_t>(vec.y);
        m_z = normalise<int16_t>(vec.z);
        m_w = normalise<int16_t>(0.0f);
    }

    vec3_normalised& operator=(glm::vec3 vec) noexcept
    {
        set(vec);
        return *this;
    }

    glm::vec3 get() const noexcept
    {
        return glm::vec3(denormalise(m_x), denormalise(m_y), denormalise(m_z));
    }

    int16_t m_x = 0;
    int16_t m_y = 0;
    int16_t m_z = 0;

private:
    // Padding for memory alignment
    int16_t m_w = 0;
};

/** Four-element normalised fixed-point vector, 16-bit elements. */
struct vec4_normalised {
    vec4_normalised() = default;
    explicit vec4_normalised(glm::vec4 vec) noexcept { set(vec); }

    explicit vec4_normalised(float x, float y, float z, float w) noexcept
    {
        set(glm::vec4{ x, y, z, w });
    }

    void set(glm::vec4 vec) noexcept
    {
        m_x = normalise<int16_t>(vec.x);
        m_y = normalise<int16_t>(vec.y);
        m_z = normalise<int16_t>(vec.z);
        m_w = normalise<int16_t>(vec.w);
    }

    vec4_normalised& operator=(glm::vec4 vec) noexcept
    {
        set(vec);
        return *this;
    }

    glm::vec4 get() const noexcept
    {
        return glm::vec4(denormalise(m_x), denormalise(m_y), denormalise(m_z), denormalise(m_w));
    }

    int16_t m_x = 0;
    int16_t m_y = 0;
    int16_t m_z = 0;
    int16_t m_w = 0;
};

} // namespace Mg
