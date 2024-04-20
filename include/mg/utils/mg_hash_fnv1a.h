//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_hash_fnv1a.h
 * FNV-1a string hashing.
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include <string_view>

/** Hash string using FNV1a algorithm. */
MG_INLINE MG_USES_UNSIGNED_OVERFLOW constexpr uint32_t hash_fnv1a(std::string_view str)
{
    uint32_t hash = 2166136261u;

    for (const char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }

    return hash;
}

