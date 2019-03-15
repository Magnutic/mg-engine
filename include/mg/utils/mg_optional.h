//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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
template<typename T> Opt<std::decay_t<T>> make_opt(T&& value)
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
