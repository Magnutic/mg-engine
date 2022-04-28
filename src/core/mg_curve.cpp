//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_curve.h"

#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_string_utils.h"

#include <cfloat>
#include <cmath>
#include <sstream>

namespace Mg {

namespace {

float distance_in_x(const ControlPoint& a, const ControlPoint& b)
{
    return (a.x > b.x) ? (b.x + (1.0f - a.x)) : (b.x - a.x);
}

constexpr float interpolate_samples(const ControlPoint* a, const ControlPoint* b, const float x)
{
    if (!a && !b) {
        return 0.0f;
    }
    if (!a) {
        return b->y;
    }
    if (!b) {
        return a->y;
    }

    const bool wrapped_around = (a->x > b->x);
    const float sample_distance = distance_in_x(*a, *b);

    if (std::abs(sample_distance) <= FLT_EPSILON) {
        return b->y;
    }

    const float factor = (wrapped_around && x < a->x ? (1.0f - a->x + x) : x - a->x) /
                         sample_distance;

    const float y_diff = std::abs(b->y - a->y);

    const float y0 = a->y;
    const float y1 = a->y + a->right_tangent * (1.0f / 3.0f) * y_diff;
    const float y2 = b->y - b->left_tangent * (1.0f / 3.0f) * y_diff;
    const float y3 = b->y;

    return cubic_bezier(y0, y1, y2, y3, factor);
}

} // namespace

Curve::Index Curve::insert(ControlPoint point)
{
    auto it = insertion_it(point.x);
    m_control_points.insert(it, point);
    return as<Index>(std::distance(m_control_points.cbegin(), it));
}

bool Curve::remove(Index index)
{
    if (index >= m_control_points.size()) {
        return false;
    }

    m_control_points.erase(m_control_points.begin() + index);
    return true;
}

void Curve::set_right_tangent(Index index, float tangent)
{
    auto& point = m_control_points.at(index);
    tangent = std::clamp(tangent, -max_tangent, max_tangent);
    point.right_tangent = tangent;
}

void Curve::set_left_tangent(Index index, float tangent)
{
    auto& point = m_control_points.at(index);
    tangent = std::clamp(tangent, -max_tangent, max_tangent);
    point.left_tangent = tangent;
}

float Curve::set_x(Index index, float x)
{
    MG_ASSERT(index < m_control_points.size());
    ControlPoint& point = m_control_points.at(index);
    const ControlPoint& point_before = m_control_points.at(wrap_around(ptrdiff_t(index) - 1));
    const ControlPoint& point_after = m_control_points.at(wrap_around(ptrdiff_t(index) + 1));

    const auto clamp_min = point_before.x > point.x ? 0.0f : point_before.x + FLT_EPSILON;
    const auto clamp_max = point_after.x < point.x ? 1.0f : point_after.x - FLT_EPSILON;
    point.x = std::clamp(x, clamp_min, clamp_max);
    return point.x;
}

float Curve::set_y(Index index, float y)
{
    auto& point = m_control_points.at(index);
    point.y = std::clamp(y, -1.0f, 1.0f);
    return point.y;
}

float Curve::evaluate(float x) const
{
    const auto& [first, second] = get_samples(x);
    return std::clamp(interpolate_samples(first, second, std::fmod(x, 1.0f)), -1.0f, 1.0f);
}

Curve::Index Curve::wrap_around(ptrdiff_t i) const
{
    const auto size = as<ptrdiff_t>(m_control_points.size());
    if (size == 0) return 0;
    while (i < 0) i += size;
    while (i >= size) i -= size;
    return as<Index>(i);
}

std::pair<const ControlPoint*, const ControlPoint*> Curve::get_samples(float x) const
{
    if (m_control_points.empty()) {
        return { nullptr, nullptr };
    }

    auto cmp = [](float sought_x, const ControlPoint& sample) { return sought_x < sample.x; };
    auto second_it = std::upper_bound(m_control_points.begin(), m_control_points.end(), x, cmp);
    const auto distance = std::distance(m_control_points.begin(), second_it);
    const auto second_index = wrap_around(distance);
    const auto first_index = wrap_around(distance - 1);

    return { &m_control_points.at(first_index), &m_control_points.at(second_index) };
}

Curve::ControlPoints::const_iterator Curve::insertion_it(float x)
{
    auto cmp = [](const ControlPoint& sample, float sought_x) { return sample.x < sought_x; };
    return std::lower_bound(m_control_points.begin(), m_control_points.end(), x, cmp);
}

std::string serialize_curve(const Curve& curve)
{
    std::stringstream stream;
    stream << "curve{";
    for (const ControlPoint& cp : curve.control_points()) {
        stream << cp.x << ',' << cp.y << ',' << cp.left_tangent << ',' << cp.right_tangent << ';';
    }
    stream << '}';
    return stream.str();
}

DeserializeCurveResult deserialize_curve(const std::string_view serialized_curve)
{
    constexpr std::string_view expected_prefix = "curve{";
    constexpr std::string_view expected_suffix = "}";

    auto error = [&](const std::string& reason) {
        return DeserializeCurveResult{
            nullopt, fmt::format("Error deserialising curve '{}': {}", serialized_curve, reason)
        };
    };

    if (!is_prefix_of(expected_prefix, serialized_curve)) {
        return error(fmt::format("expected prefix '{}'", expected_prefix));
    }
    if (!is_suffix_of(expected_suffix, serialized_curve)) {
        return error(fmt::format("expected suffix '{}'", expected_suffix));
    }

    std::string_view input = split_string_on_char(serialized_curve, '{').second;
    input = substring_until(input, '}');

    auto next_control_point = [&input]() -> Opt<std::array<float, 4>> {
        std::array<float, 4> control_point{};

        std::string_view points_string;
        std::tie(points_string, input) = split_string_on_char(input, ';');

        auto values = tokenize_string(points_string, ",");
        if (values.size() != control_point.size()) {
            return nullopt;
        }

        auto parseValue = [](std::string_view value) { return string_to<float>(value).second; };
        std::transform(values.begin(), values.end(), control_point.begin(), parseValue);
        return control_point;
    };

    Curve result;

    for (auto control_point = next_control_point(); control_point.has_value();
         control_point = next_control_point()) {
        const auto& [x, y, left_tangent, right_tangent] = control_point.value();
        result.unsafe_insert_at_end(ControlPoint{ x, y, left_tangent, right_tangent });
    }

    return { result, "" };
}

} // namespace Mg
