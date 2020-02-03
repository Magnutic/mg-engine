//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_angle.h
 * Type representing a geometric angle.
 */

#pragma once

#include <cmath>

namespace Mg {

class Angle {
public:
    enum class Unit { degree, radian };

    constexpr Angle() {}

    constexpr Angle(Unit unit, float angle)
        : m_angle_radians(angle * (unit == Unit::degree) * deg_to_rad)
    {}

    constexpr float degrees() const noexcept { return rad_to_deg * m_angle_radians; }
    constexpr float radians() const noexcept { return m_angle_radians; }

    constexpr friend Angle operator+(Angle lhs, Angle rhs) noexcept
    {
        return { lhs.radians() + rhs.radians() };
    }
    constexpr friend Angle operator-(Angle lhs, Angle rhs) noexcept
    {
        return { lhs.radians() - rhs.radians() };
    }
    constexpr friend Angle operator*(Angle lhs, float f) noexcept { return { lhs.radians() * f }; }
    constexpr friend Angle operator/(Angle lhs, float f) noexcept { return { lhs.radians() / f }; }

    friend Angle angle_difference(Angle lhs, Angle rhs) noexcept
    {
        const Angle tmp = rhs - lhs;
        return std::remainder(tmp.radians() + pi, two_pi) - pi;
    }

    constexpr Angle wrap_0_to_360() const noexcept { return { std::remainder(radians(), two_pi) }; }
    constexpr Angle wrap_neg_180_to_180() const noexcept { return wrap_0_to_360() - pi; };

    constexpr Angle& operator+=(Angle rhs) noexcept
    {
        (*this) = (*this) + rhs;
        return *this;
    }
    constexpr Angle& operator-=(Angle rhs) noexcept
    {
        (*this) = (*this) - rhs;
        return *this;
    }
    constexpr Angle& operator*=(float f) noexcept
    {
        (*this) = (*this) * f;
        return *this;
    }
    constexpr Angle& operator/=(float f) noexcept
    {
        (*this) = (*this) / f;
        return *this;
    }

private:
    // Internal constructor allows operations that always work in radians to skip conditional in
    // public constructor.
    constexpr Angle(float angle_radians) : m_angle_radians(angle_radians) {}

    static constexpr float pi = 3.14159265358979323846233f;
    static constexpr float two_pi = 2.0f * pi;
    static constexpr float deg_to_rad = pi / 180.0f;
    static constexpr float rad_to_deg = 180.0f / pi;

    float m_angle_radians = 0.0;
};

namespace literals {

constexpr Angle operator""_degrees(long double degrees)
{
    return Angle(Angle::Unit::degree, static_cast<float>(degrees));
}
constexpr Angle operator""_degrees(unsigned long long degrees)
{
    return Angle(Angle::Unit::degree, static_cast<float>(degrees));
}
constexpr Angle operator""_radians(long double radians)
{
    return Angle(Angle::Unit::radian, static_cast<float>(radians));
}
constexpr Angle operator""_radians(unsigned long long radians)
{
    return Angle(Angle::Unit::radian, static_cast<float>(radians));
}

} // namespace literals

using namespace literals;

} // namespace Mg
