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

/** @file mg_vector_normalised.h
 * Normalised vector types: uses integral numeric types where the range of values is interpreted as
 * a fixed-point value in [0.0, 1.0].
 */

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mg {

/** Creates a "normalised" integer of T from a floating point value in the range
 * [-1.0f, 1.0f] by using T's entire range as fixed-point representation. For
 * use with OpenGL's normalised vertex attributes.
 *
 * For input values outside the range the fractional part is used.
 */
template<typename T> inline T normalise(float value)
{
    static_assert(std::is_integral<T>::value, "Can only normalise to integers");

    // Wrap around if value is outside normalised range
    if (value > 1.0f) {
        value -= static_cast<T>(value);
    }
    else if (value < -1.0f) {
        value += static_cast<T>(value);
    }

    // Multiply by max to obtain normalised value
    return static_cast<T>(value * std::numeric_limits<T>::max());
}

/** Denormalise an integer by converting its range to a float [-1.0f, 1.0f]. */
template<typename T> constexpr float denormalise(T value)
{
    static_assert(std::is_integral<T>::value, "Can only denormalise integers");

    // Divide by max to obtain denormalised value
    return static_cast<float>(value) / std::numeric_limits<T>::max();
}

/** Two-element normalised fixed-point vector, 16-bit elements. */
struct vec2_normalised {
    vec2_normalised() = default;
    explicit vec2_normalised(glm::vec2 vec) { set(vec); }
    explicit vec2_normalised(float x, float y) { set(glm::vec2{ x, y }); }

    void set(glm::vec2 vec)
    {
        m_x = normalise<int16_t>(vec.x);
        m_y = normalise<int16_t>(vec.y);
    }

    vec2_normalised& operator=(glm::vec2 vec)
    {
        set(vec);
        return *this;
    }

    glm::vec2 get() const { return glm::vec2(denormalise(m_x), denormalise(m_y)); }

    int16_t m_x = 0;
    int16_t m_y = 0;
};

/** Three-element normalised fixed-point vector, 16-bit elements. */
struct vec3_normalised {
    vec3_normalised() = default;
    explicit vec3_normalised(glm::vec3 vec) { set(vec); }

    explicit vec3_normalised(float x, float y, float z) { set(glm::vec3{ x, y, z }); }

    void set(glm::vec3 vec)
    {
        m_x = normalise<int16_t>(vec.x);
        m_y = normalise<int16_t>(vec.y);
        m_z = normalise<int16_t>(vec.z);
        m_w = normalise<int16_t>(0.0f);
    }

    vec3_normalised& operator=(glm::vec3 vec)
    {
        set(vec);
        return *this;
    }

    glm::vec3 get() const
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
    explicit vec4_normalised(glm::vec4 vec) { set(vec); }

    explicit vec4_normalised(float x, float y, float z, float w) { set(glm::vec4{ x, y, z, w }); }

    void set(glm::vec4 vec)
    {
        m_x = normalise<int16_t>(vec.x);
        m_y = normalise<int16_t>(vec.y);
        m_z = normalise<int16_t>(vec.z);
        m_w = normalise<int16_t>(vec.w);
    }

    vec4_normalised& operator=(glm::vec4 vec)
    {
        set(vec);
        return *this;
    }

    glm::vec4 get() const
    {
        return glm::vec4(denormalise(m_x), denormalise(m_y), denormalise(m_z), denormalise(m_w));
    }

    int16_t m_x = 0;
    int16_t m_y = 0;
    int16_t m_z = 0;
    int16_t m_w = 0;
};

} // namespace Mg
