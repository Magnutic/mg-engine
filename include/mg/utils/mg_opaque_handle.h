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

/** @file mg_opaque_handle.h
 * Move-only wrapper for 64-bit handles. This is useful for wrapping handles to graphics API
 * objects.
 */

#pragma once

#include <cstdint>
#include <utility>

namespace Mg {

/** Move-only wrapper for 64-bit handles. This is useful for wrapping handles to graphics API
 * objects.
 */
class OpaqueHandle {
public:
    enum class Value : uint64_t;
    Value value{ 0 };

    OpaqueHandle() = default;

    OpaqueHandle(uint64_t id) : value{ id } {}

    OpaqueHandle(OpaqueHandle&& rhs) noexcept : value{ rhs.value } { rhs.value = {}; }

    OpaqueHandle& operator=(OpaqueHandle&& rhs) noexcept
    {
        swap(rhs);
        OpaqueHandle tmp;
        rhs.swap(tmp);
        return *this;
    }

    OpaqueHandle(const OpaqueHandle&) = delete;
    OpaqueHandle& operator=(const OpaqueHandle&) = delete;

    void swap(OpaqueHandle& rhs) noexcept { std::swap(value, rhs.value); }

    friend bool operator==(const OpaqueHandle& lhs, const OpaqueHandle& rhs)
    {
        return lhs.value == rhs.value;
    }

    friend bool operator!=(const OpaqueHandle& lhs, const OpaqueHandle& rhs)
    {
        return lhs.value != rhs.value;
    }
};

} // namespace Mg
