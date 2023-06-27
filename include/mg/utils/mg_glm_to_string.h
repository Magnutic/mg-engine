//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2023, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_glm_to_string.h
 * to_string for glm types.
 */

#pragma once

#include <glm/fwd.hpp>

#include <string>

namespace Mg {

template<glm::length_t L, typename T, glm::qualifier Q>
std::string to_string(const glm::vec<L, T, Q>& vec)
{
    std::string result = "{ ";
    for (glm::length_t i = 0; i < L; ++i) {
        const bool is_last = i + 1 == L;
        result += std::to_string(vec[i]);
        if (!is_last) {
            result += ", ";
        }
    }
    result += " }";
    return result;
}

template<glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
std::string to_string(const glm::mat<C, R, T, Q>& mat)
{
    std::string result = "{\n";
    for (glm::length_t r = 0; r < R; ++r) {
        result += "\n  ";
        for (glm::length_t c = 0; c < C; ++c) {
            const bool is_last = c + 1 == C;
            result += std::to_string(mat[c][r]);
            if (!is_last) {
                result += ", ";
            }
        }
    }
    result += "\n}\n";
    return result;
}

template<typename T, glm::qualifier Q> std::string to_string(const glm::qua<T, Q>& qua)
{
    return "{ w: " + std::to_string(qua.w) + ", x: " + std::to_string(qua.x) +
           ", y: " + std::to_string(qua.y) + ", z: " + std::to_string(qua.z) + " }";
}

} // namespace Mg
