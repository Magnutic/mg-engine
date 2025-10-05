//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_is_instantiation_of.h
 * Type trait to determine whether a type is an instantiation of a given class template.
 */

#pragma once

#include <type_traits>

namespace Mg {

template<typename T, template<typename...> typename U>
inline constexpr bool is_instantiation_of_v = std::false_type{};

template<template<typename...> typename U, typename... Vs>
inline constexpr bool is_instantiation_of_v<U<Vs...>, U> = std::true_type{};

template<typename T, template<typename...> typename U>
concept InstantiationOf = is_instantiation_of_v<T, U>;

} // namespace Mg
