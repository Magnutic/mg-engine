//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/utils/mg_rand.h"

#include <cfloat>
#include <cmath>

namespace Mg {

constexpr float pi_f = 3.14159265359f;
constexpr double pi_d = 3.14159265358979323462f;

/** Get random float with normal distribution. */
float Random::normal_distributed(const float mean, const float deviation)
{
    float u1 = 0.0f;
    float u2 = 0.0f;
    do {
        u1 = f32();
        u2 = f32();
    } while (u1 <= FLT_EPSILON);

    // Box-Muller transform to convert two uniform random variables into a normal-distributed one.
    return mean + deviation * std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * pi_f * u2);
}

/** Get random float with normal distribution. */
double Random::normal_distributed(const double mean, const double deviation)
{
    double u1 = 0.0;
    double u2 = 0.0;
    do {
        u1 = f64();
        u2 = f64();
    } while (u1 <= DBL_EPSILON);

    // Box-Muller transform to convert two uniform random variables into a normal-distributed one.
    return mean + deviation * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * pi_d * u2);
}

} // namespace Mg
