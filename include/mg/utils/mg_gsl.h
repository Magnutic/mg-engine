//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
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
#include <type_traits>
#include <utility>

#ifndef MG_CHECK_SPAN_ACCESS
/** Whether to do bounds check when accessing span elements. */
#    define MG_CHECK_SPAN_ACCESS 1
#endif

#ifndef MG_CHECK_SPAN_CONSTRUCTION
/** Whether to do a run-time sanity check on span constructors. */
#    define MG_CHECK_SPAN_CONSTRUCTION 1
#endif

#if (__GNUC__ >= 7) || (__clang_major__ >= 5) || (_MSC_VER >= 1914)
#    define MG_HAVE_CLASS_TEMPLATE_DEDUCTION 1
#else
#    define MG_HAVE_CLASS_TEMPLATE_DEDUCTION 0
#endif

// For the sake of testing, we often want to check that assertions are triggered. This is done by
// catching exceptions. In tests, assertion failures throw, which means that otherwise-noexcept
// functions must be non-noexcept, hence this macro.
#if MG_CONTRACT_VIOLATION_THROWS
#    define MG_SPAN_NOEXCEPT
#else
#    define MG_SPAN_NOEXCEPT noexcept
#endif

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
    MG_ASSERT(static_cast<From>(ret_val) == value &&
              "Narrowing conversion resulted in changed value.");
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
// GSL.view
//
// `span`, non-owning view over memory contiguous data.
//--------------------------------------------------------------------------------------------------

/** Non-owning view over memory-contiguous data of a single type. Based on gsl::span.
 * Deliberately limited to dynamic-extent spans.
 */
template<typename T> class span {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using pointer = T*;
    using reference = T&;

    using iterator = T*;
    using const_iterator = const T*;

    //----------------------------------------------------------------------------------------------
    // Constructors
    //----------------------------------------------------------------------------------------------

    constexpr span() noexcept : m_begin(nullptr), m_end(nullptr) {}

    constexpr span(std::nullptr_t, size_type len = 0) MG_SPAN_NOEXCEPT : span()
    {
        if constexpr (MG_CHECK_SPAN_CONSTRUCTION) {
            MG_ASSERT(len == 0);
        }
    }

    // Basic, safety-checking constructor. All other constructors delegate to this one.
    template<typename U> constexpr span(U* begin, U* end) MG_SPAN_NOEXCEPT
    {
        // N.B. these static assert checks are stricter than normal type checking on pointer
        // assignment, as it prevents pointers to subclasses of T (and thus prevents slicing).
        static_assert(std::is_const_v<T> || !std::is_const_v<U>,
                      "span construction: cannot to construct non-const span over const values.");

        static_assert(std::is_convertible_v<U*, T*>, "span construction: incompatible types.");

        static_assert(std::is_same_v<std::decay_t<T>, std::decay_t<U>> || !std::is_base_of_v<T, U>,
                      "span construction: attempting to construct span using a subclass of span's "
                      "type. This would invoke the slicing problem.");

        m_begin = begin; // Not using initialisation lists in order to let static_asserts trigger
        m_end = end;     // before pointer assignment type errors (gives clearer error messages).

        if constexpr (MG_CHECK_SPAN_CONSTRUCTION) {
            sanity_check();
        }
    }

    template<typename U>
    constexpr span(U* p, size_type len) MG_SPAN_NOEXCEPT
        : span((len > 0) ? p : nullptr, (len > 0) ? p + len : nullptr) // NOLINT
    {
        MG_ASSERT(p || len == 0);
    }

    template<typename U>
    constexpr span(const span<U>& rhs) MG_SPAN_NOEXCEPT : span(rhs.begin(), rhs.end())
    {}

    template<typename ContainerT> // requires ContiguousContainer<ContainerT>
    constexpr span(ContainerT& c) MG_SPAN_NOEXCEPT
        : span(c.data(), narrow_cast<size_type>(c.size()))
    {}

    template<typename U, size_t N>
    constexpr span(U (&array)[N]) MG_SPAN_NOEXCEPT // NOLINT(cppcoreguidelines-avoid-c-arrays)
        : span(&array[0], &array[0] + N)
    {}

    template<typename U> constexpr span& operator=(const span<U>& rhs) noexcept
    {
        m_begin = rhs.m_begin;
        m_end = rhs.m_end;
        return *this;
    }

    //----------------------------------------------------------------------------------------------
    // Element access
    //----------------------------------------------------------------------------------------------
    constexpr T& operator[](size_type i) const MG_SPAN_NOEXCEPT
    {
        if constexpr (MG_CHECK_SPAN_ACCESS) {
            MG_ASSERT(i < size());
        }
        return *(m_begin + i);
    }

    constexpr T* data() const noexcept { return m_begin; }

    //----------------------------------------------------------------------------------------------
    // Subspans
    //----------------------------------------------------------------------------------------------
    constexpr span subspan(size_type offset, size_type count) const MG_SPAN_NOEXCEPT
    {
        MG_ASSERT(offset + count <= size());
        return span(m_begin + offset, m_begin + offset + count);
    }

    constexpr span subspan(size_type offset) const MG_SPAN_NOEXCEPT
    {
        MG_ASSERT(offset <= size());
        return subspan(offset, size() - offset);
    }

    constexpr span first(size_type n) const MG_SPAN_NOEXCEPT { return subspan(0, n); }

    constexpr span last(size_type n) const MG_SPAN_NOEXCEPT
    {
        MG_ASSERT(n <= size());
        return subspan(size() - n, n);
    }

    //----------------------------------------------------------------------------------------------
    // Observers
    //----------------------------------------------------------------------------------------------
    constexpr size_type size() const noexcept { return narrow_cast<size_type>(m_end - m_begin); }
    constexpr size_type length() const noexcept { return size(); }

    constexpr bool empty() const noexcept { return m_end == m_begin; }

    constexpr size_type size_bytes() const noexcept { return size() * elem_size; }

    constexpr size_type length_bytes() const noexcept { return size_bytes(); }


    //----------------------------------------------------------------------------------------------
    // Iterator access
    //----------------------------------------------------------------------------------------------
    constexpr iterator begin() const noexcept { return m_begin; }
    constexpr const_iterator cbegin() const noexcept { return m_begin; }

    constexpr iterator end() const noexcept { return m_end; }
    constexpr const_iterator cend() const noexcept { return m_end; }

    //----------------------------------------------------------------------------------------------

    constexpr span<const std::byte> as_bytes() const noexcept
    {
        return span<const std::byte>(reinterpret_cast<const std::byte*>(m_begin), size_bytes());
    }

private:
    template<typename U> friend class span;

    constexpr void sanity_check() MG_SPAN_NOEXCEPT
    {
        MG_ASSERT((m_begin && m_end) || (!m_begin && !m_end));
        MG_ASSERT(m_begin <= m_end);
    }

    static constexpr size_type elem_size = narrow<size_type>(sizeof(T));

    T* m_begin;
    T* m_end;
};

// Sanity check
static_assert(std::is_trivially_copyable_v<span<int>>);

// Template type deduction guides. Mirrors constructors.
// clang-format off
#if MG_HAVE_CLASS_TEMPLATE_DEDUCTION
template<typename U          > span(U*, U*)     -> span<U>;
template<typename U          > span(U*, size_t) -> span<U>;
template<typename U, size_t N> span(U (&)[N])   -> span<U>; // NOLINT(cppcoreguidelines-avoid-c-arrays)

// Deduce to the pointee type of the result of calling data() on a variable of the container type.
template<typename ContainerT> // requires ContiguousContainer<ContainerT>
span(ContainerT&) -> span<std::remove_pointer_t<decltype(std::declval<ContainerT>().data())>>;
// clang-format on
#endif

//--------------------------------------------------------------------------------------------------
// Object (byte) representation viewing functions.
//--------------------------------------------------------------------------------------------------

/** Reinterpret span of standard-layout object as a span of bytes. */
template<typename T> span<const std::byte> as_bytes(span<T> span) noexcept
{
    return span.as_bytes();
}

// as_writeable_bytes omitted: does that not violate strict aliasing rules?

/** Reinterpret any standard-layout object as a span of bytes. */
template<typename T> span<const std::byte> byte_representation(const T& obj) noexcept
{
    static_assert(std::is_standard_layout_v<T>);
    return { reinterpret_cast<const std::byte*>(&obj), sizeof(obj) };
}

//--------------------------------------------------------------------------------------------------
// final_action and finally,
//--------------------------------------------------------------------------------------------------

/** final_action: RAII utility that runs the callable with which it was constructed at the end of
 * the its scope.
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
template<typename F, typename ActionT = final_action<std::remove_cv_t<std::remove_reference_t<F>>>>
[[nodiscard]] ActionT finally(F&& f) noexcept
{
    return ActionT(std::forward<F>(f));
}

} // namespace Mg::gsl

// Bring GSL implementation into Mg namespace for convenience.
namespace Mg {
using ::Mg::gsl::as_bytes;
using ::Mg::gsl::at;
using ::Mg::gsl::byte_representation;
using ::Mg::gsl::final_action;
using ::Mg::gsl::finally;
using ::Mg::gsl::narrow;
using ::Mg::gsl::narrow_cast;
using ::Mg::gsl::span;
} // namespace Mg

#undef MG_SPAN_NOEXCEPT
