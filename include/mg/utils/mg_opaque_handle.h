//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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
    using Value = uint64_t;
    Value value{ 0 };

    OpaqueHandle() = default;

    OpaqueHandle(uint64_t id) noexcept : value{ id } {}

    OpaqueHandle(OpaqueHandle&& rhs) noexcept : value{ rhs.value } { rhs.value = {}; }

    OpaqueHandle& operator=(OpaqueHandle&& rhs) noexcept
    {
        OpaqueHandle tmp{ std::move(rhs) };
        swap(tmp);
        return *this;
    }

    OpaqueHandle(const OpaqueHandle&) = delete;
    OpaqueHandle& operator=(const OpaqueHandle&) = delete;

    ~OpaqueHandle() = default;

    void swap(OpaqueHandle& rhs) noexcept { std::swap(value, rhs.value); }

    friend bool operator==(const OpaqueHandle& lhs, const OpaqueHandle& rhs) noexcept
    {
        return lhs.value == rhs.value;
    }

    friend bool operator!=(const OpaqueHandle& lhs, const OpaqueHandle& rhs) noexcept
    {
        return lhs.value != rhs.value;
    }
};

} // namespace Mg
