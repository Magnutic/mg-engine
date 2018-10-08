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

/** @file mg_object_id.h
 * Move-only numeric id wrapper. Useful for e.g. unique ownership of OpenGL objects.
 */

#pragma once

#include <cstdint>

namespace Mg {

/** Wrapper for numeric object identifiers which prevents copying and zeros id value upon move
 * operations. This allows object wrapper classes to be safely default-movable.
 */
class ObjectId {
public:
    /** Wrapped identifier value. */
    uint32_t value = 0;

    ObjectId() = default;

    explicit ObjectId(uint32_t id) : value{ id } {}

    ObjectId(ObjectId&& rhs) noexcept : value{ rhs.value } { rhs.value = 0; }

    ObjectId& operator=(uint32_t id)
    {
        value = id;
        return *this;
    }

    ObjectId& operator=(ObjectId&& rhs) noexcept
    {
        value     = rhs.value;
        rhs.value = 0;
        return *this;
    }

    friend bool operator==(const ObjectId& lhs, const ObjectId& rhs)
    {
        return lhs.value == rhs.value;
    }

    ObjectId(const ObjectId&) = delete;
    ObjectId& operator=(const ObjectId&) = delete;
};

} // namespace Mg
