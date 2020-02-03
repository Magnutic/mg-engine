//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_optional.
 * Wrapper header to include the `optional` class template and provide `Mg::Opt` as a type alias.
 */

#pragma once

#include <tl/optional.hpp>

#include <type_traits>
#include <utility>

namespace Mg {

/** Optional type. Currently an alias of tl::optional, but may switch to std::optional in the
 * future, if the required features land in the standard library.
 */
template<typename T> using Opt = tl::optional<T>;

using tl::bad_optional_access;
using tl::in_place;
using tl::in_place_t;
using tl::monostate;
using tl::nullopt;
using tl::nullopt_t;

/** Alias for tl::make_optional() which avoids ambiguities from argument-dependent lookup by having
 * a different name.
 * Refer to documentation for std::optional or tl::optional for details.
 */
template<typename T> constexpr Opt<std::decay_t<T>> make_opt(T&& value)
{
    return tl::make_optional<T>(std::forward<T>(value));
}

/** Alias for tl::make_optional() which avoids ambiguities from argument-dependent lookup by having
 * a different name.
 * Refer to documentation for std::optional or tl::optional for details.
 */
template<typename T, typename... Ts> Opt<T> make_opt(Ts&&... args)
{
    return tl::make_optional<T>(std::forward<Ts>(args)...);
}

} // namespace Mg
