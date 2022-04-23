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

    static constexpr Angle from_radians(float angle) { return { Angle::Unit::radian, angle }; }
    static constexpr Angle from_degrees(float angle) { return { Angle::Unit::degree, angle }; }

    static constexpr Angle clamp(Angle v, Angle low, Angle high) noexcept
    {
        if (v.radians() < low.radians()) {
            return low;
        }
        if (v.radians() > high.radians()) {
            return high;
        }
        return v;
    }

    constexpr Angle() = default;

    constexpr Angle(Unit unit, float angle)
        : m_angle_radians((unit == Unit::degree) ? (angle * deg_to_rad) : angle)
    {}

    constexpr Angle(const Angle&) = default;
    constexpr Angle& operator=(const Angle&) = default;
    constexpr Angle(Angle&&) = default;
    constexpr Angle& operator=(Angle&&) = default;

    ~Angle() = default;

    constexpr float degrees() const noexcept { return rad_to_deg * m_angle_radians; }
    constexpr float radians() const noexcept { return m_angle_radians; }

    constexpr Angle wrap_0_to_360() const noexcept
    {
        return Angle{ std::remainder(radians(), two_pi) };
    }
    constexpr Angle wrap_neg_180_to_180() const noexcept { return wrap_0_to_360() - Angle{ pi }; };

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

    constexpr friend Angle operator-(Angle a) noexcept { return Angle{ -a.radians() }; }

    constexpr friend Angle operator+(Angle a) noexcept { return a; }

    constexpr friend Angle operator+(Angle lhs, Angle rhs) noexcept
    {
        return Angle{ lhs.radians() + rhs.radians() };
    }
    constexpr friend Angle operator-(Angle lhs, Angle rhs) noexcept
    {
        return Angle{ lhs.radians() - rhs.radians() };
    }

    constexpr friend Angle operator*(Angle lhs, float f) noexcept
    {
        return Angle{ lhs.radians() * f };
    }
    constexpr friend Angle operator*(float f, Angle rhs) noexcept
    {
        return Angle{ rhs.radians() * f };
    }
    constexpr friend Angle operator/(Angle lhs, float f) noexcept
    {
        return Angle{ lhs.radians() / f };
    }

    friend Angle angle_difference(Angle lhs, Angle rhs) noexcept
    {
        const Angle tmp = rhs - lhs;
        return Angle{ std::remainder(tmp.radians() + pi, two_pi) - pi };
    }

private:
    // Internal constructor allows operations that always work in radians to skip conditional in
    // public constructor.
    constexpr explicit Angle(float angle_radians) : m_angle_radians(angle_radians) {}

    static constexpr float pi = 3.14159265358979323846233f;
    static constexpr float two_pi = 2.0f * pi;
    static constexpr float deg_to_rad = pi / 180.0f;
    static constexpr float rad_to_deg = 180.0f / pi;

    float m_angle_radians = 0.0f;
};

namespace literals {

constexpr Angle operator""_degrees(long double degrees)
{
    return { Angle::Unit::degree, static_cast<float>(degrees) };
}
constexpr Angle operator""_degrees(unsigned long long degrees)
{
    return { Angle::Unit::degree, static_cast<float>(degrees) };
}
constexpr Angle operator""_radians(long double radians)
{
    return { Angle::Unit::radian, static_cast<float>(radians) };
}
constexpr Angle operator""_radians(unsigned long long radians)
{
    return { Angle::Unit::radian, static_cast<float>(radians) };
}

} // namespace literals

using namespace literals;

} // namespace Mg
