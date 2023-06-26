//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file core/mg_curve.h
 * 2D curve resource that contains a variable number of control points and which can be evaluated at
 * any X coordinate.
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_optional.h"

#include <string>
#include <string_view>

namespace Mg {

struct ControlPoint {
    float x = 0.0f;
    float y = 0.0f;
    float left_tangent = 0.0f;
    float right_tangent = 0.0f;
};

/** 2D curve resource that contains a variable number of control points and which can be evaluated
 * at any X coordinate.
 */
class Curve {
public:
    using Index = size_t;
    static constexpr float max_tangent = 50.0f;

    Index insert(ControlPoint point);

    void unsafe_insert_at_end(ControlPoint point) { m_control_points.push_back(point); }

    bool remove(Index index);

    void set_left_tangent(Index index, float tangent);

    void set_right_tangent(Index index, float tangent);

    // Move the point at the given index to the given position in the x axis. If another point is in
    // the way, the movement will be clamped so that this point does not bypass the other. The value
    // of x will be clamped to [0, 1].
    // Returns new x, which might be different from requested if the movement was clamped.
    float set_x(Index index, float x);

    // Move the point at the given index to the given position in the y axis. The value of y will be
    // clamped to [-1, 1].
    // Returns new y, which might be different from requested if the movement was clamped.
    float set_y(Index index, float y);

    std::span<const ControlPoint> control_points() const { return m_control_points; }

    float evaluate(float x) const;

private:
    using ControlPoints = small_vector<ControlPoint, 10>;

    Index wrap_around(ptrdiff_t i) const;

    std::pair<const ControlPoint*, const ControlPoint*> get_samples(float x) const;

    ControlPoints::const_iterator insertion_it(float x);

    ControlPoints m_control_points;
};

std::string serialize_curve(const Curve& curve);

struct DeserializeCurveResult {
    Opt<Curve> curve;
    std::string error;
};

DeserializeCurveResult deserialize_curve(std::string_view serialized_curve);

} // namespace Mg
