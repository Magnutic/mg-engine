//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_gsl.h
 * Minimal C++17 implementation of Guideline Support Library utilities.
 * Note that this contains only a subset of GSL features.
 */

#pragma once

#include "mg/utils/mg_assert.h"

#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

namespace Mg::gsl {

//--------------------------------------------------------------------------------------------------
// GSL.util
//
//--------------------------------------------------------------------------------------------------

// Narrowing casts

// Disable sign-conversion warnings due to GCC bug that triggers warnings despite explicit casts.
#if __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

/** Cast to narrower type. Equivalent to static_cast but conveys intention. */
template<typename To, typename From> constexpr To narrow_cast(From value) noexcept
{
    return static_cast<To>(value);
}

/** Cast to narrower type and assert that the resulting value is equivalent to input. */
template<typename To, typename From> constexpr To narrow(From value)
{
    auto ret_val = narrow_cast<To>(value);
    MG_ASSERT(static_cast<From>(ret_val) == value && "conversion resulted in changed value.");
    MG_ASSERT(std::is_signed_v<To> == std::is_signed_v<From> ||
              (ret_val < To{}) == (value < From{}));
    return ret_val;
}

#if __GNUC__
#    pragma GCC diagnostic pop
#endif

// gsl::at(): Bounds checking subscript

template<typename T, size_t N>
auto& at(T (&array)[N], size_t index) // NOLINT(cppcoreguidelines-avoid-c-arrays)
{
    MG_ASSERT(index < N);
    return array[index];
}

template<typename T> auto& at(T& container, size_t index)
{
    MG_ASSERT(index < container.size());
    return container[index];
}

//--------------------------------------------------------------------------------------------------
// final_action and finally,
//--------------------------------------------------------------------------------------------------

/** final_action: RAII utility that, at the end of the its scope, runs the callable with which it
 * was constructed.
 */
template<typename F> class final_action {
public:
    static_assert(!std::is_reference_v<F> && !std::is_const_v<F> && !std::is_volatile_v<F>);

    [[nodiscard]] explicit final_action(F f) noexcept : m_action(std::move(f)) {}

    [[nodiscard]] final_action(final_action&& other) noexcept
        : m_action(std::move(other.m_action))
        , m_should_invoke(std::exchange(other.m_should_invoke, false))
    {}

    final_action(const final_action&) = delete;

    final_action& operator=(const final_action&) = delete;
    final_action& operator=(final_action&&) = delete;

    ~final_action() noexcept
    {
        if (m_should_invoke) {
            m_action();
        }
    }

private:
    F m_action;
    bool m_should_invoke = true;
};

/** Helper function to construct a final_action. */
template<typename F, typename ActionT = final_action<std::remove_cvref_t<F>>>
[[nodiscard]] ActionT finally(F&& f) noexcept
{
    return ActionT(std::forward<F>(f));
}

} // namespace Mg::gsl

// Bring GSL implementation into Mg namespace for convenience.
namespace Mg {

using ::Mg::gsl::at;
using ::Mg::gsl::final_action;
using ::Mg::gsl::finally;
using ::Mg::gsl::narrow;
using ::Mg::gsl::narrow_cast;

// Alternative shorter name for narrow:

/** Cast to another type and assert that the resulting value remains unchanged. */
template<typename To, typename From> constexpr To as(From value)
{
    return ::Mg::gsl::narrow<To, From>(value);
}


/** Reinterpret any standard-layout object as a std::span of bytes. */
template<typename T> std::span<const std::byte> byte_representation(const T& obj) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>);
    return { reinterpret_cast<const std::byte*>(&obj), sizeof(obj) };
}

} // namespace Mg
