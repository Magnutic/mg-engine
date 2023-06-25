//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file utils/mg_enum.h
 * Macros and utilities addressing the deficiencies relating to enumeration types in C++.
 */

#pragma once

#include <mg/utils/mg_gsl.h>
#include <mg/utils/mg_optional.h>
#include <mg/utils/mg_string_utils.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#define MG_EXPAND_PARENTHESISED_ARGS(...) __VA_ARGS__
#define MG_PARENTHESISED_ARGS_AS_STRING(...) #__VA_ARGS__

/** Define an enumeration type in a manner that allows utilities in Mg::enum_utils to work for the
 * enumeration.
 * Limitations: may not include initializers (as in `enum class MyEnum { a = 1, b = 2 }`).
 * Example: `MG_ENUM(MyEnumType, (first_value, second_value, third_value))`
 */
#define MG_ENUM(name_, values_)                                                                  \
    enum class name_ : uint32_t { MG_EXPAND_PARENTHESISED_ARGS values_ };                        \
    constexpr std::string_view enum_values_specification_string(name_ /*dummy*/)                 \
    {                                                                                            \
        return MG_PARENTHESISED_ARGS_AS_STRING values_;                                          \
    }                                                                                            \
    static_assert(enum_values_specification_string(name_{}).find('=') == std::string_view::npos, \
                  "MG_ENUM(): the enumeration definition may not include initializers.");        \
    static_assert(!enum_values_specification_string(name_{}).empty(),                            \
                  "MG_ENUM(): the enumeration definition may not be empty.");

namespace Mg::enum_utils {

namespace detail {
template<typename EnumT> constexpr size_t parse_count()
{
    size_t count = 1;
    auto str = enum_values_specification_string(EnumT{});
    for (const char c : str) {
        if (c == ',') ++count;
    }

    // Compensate for trailing comma
    if (!str.empty() && str.back() == ',') --count;

    return count;
}
} // namespace detail

template<typename EnumT> constexpr size_t count = detail::parse_count<EnumT>();

namespace detail {
template<typename EnumT> constexpr auto parse_names()
{
    const auto names_string = enum_values_specification_string(EnumT{});

    std::array<std::string_view, count<EnumT>> result;
    auto it = names_string.begin();

    for (size_t i = 0; i < count<EnumT>; ++i) {
        while (*it && (is_whitespace(*it) || *it == ',')) ++it;
        const auto begin = it;

        while (*it && (is_not_whitespace(*it) && *it != ',')) ++it;
        const auto size = as<size_t>(std::distance(begin, it));

        result[i] = std::string_view(&*begin, size);
    }

    return result;
}

template<typename EnumT>
constexpr std::array<std::string_view, count<EnumT>> names = parse_names<EnumT>();

// Fallback for invalid types, used in assert_is_mg_enum().
template<typename EnumT>
constexpr std::string_view enum_values_specification_string(EnumT /*dummy*/)
{
    return "";
}

template<typename EnumT>
constexpr bool is_mg_enum = enum_values_specification_string(EnumT{}) != "";

template<typename EnumT> constexpr void assert_is_mg_enum()
{
    static_assert(
        is_mg_enum<EnumT>,
        "Mg::enum_utils: the enumeration type must be declared using the macro MG_ENUM().");
}
} // namespace detail

template<typename EnumT> constexpr std::string_view to_string(EnumT enum_value)
{
    detail::assert_is_mg_enum<EnumT>();
    return detail::names<EnumT>[static_cast<size_t>(enum_value)];
}

template<typename EnumT> constexpr Opt<EnumT> from_string(std::string_view str)
{
    detail::assert_is_mg_enum<EnumT>();
    for (size_t i = 0; i < count<EnumT>; ++i) {
        const std::string_view name = detail::names<EnumT>[i];
        if (str == name) return static_cast<EnumT>(i);
    }
    return nullopt;
}

template<typename EnumT, typename T>
class Map // requires(std::default_initializable<T>)
{
public:
    using value_type = std::pair<EnumT, T>;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    static_assert(
        detail::is_mg_enum<EnumT>,
        "Mg::enum_utils: the enumeration type must be declared using the macro MG_ENUM().");

    constexpr Map()
    {
        for (size_t i = 0; i < count<EnumT>; ++i) {
            m_map[i].first = as<EnumT>(i);
        }
    }

    constexpr Map(std::initializer_list<value_type> values) : Map()
    {
        for (const auto& [key, value] : values) {
            m_map[as<size_t>(key)].second = value;
        }
    }

    constexpr T& operator[](EnumT key) { return m_map[as<size_t>(key)].second; }
    constexpr const T& operator[](EnumT key) const { return m_map[as<size_t>(key)].second; }

    constexpr size_t size() const { return count<EnumT>; }

    constexpr iterator begin() { return m_map.begin(); }
    constexpr iterator end() { return m_map.end(); }
    constexpr const_iterator begin() const { return m_map.begin(); }
    constexpr const_iterator end() const { return m_map.end(); }
    constexpr const_iterator cbegin() const { return m_map.cbegin(); }
    constexpr const_iterator cend() const { return m_map.cend(); }

private:
    std::array<value_type, count<EnumT>> m_map;
};

} // namespace Mg::enum_utils

namespace Mg::test {

MG_ENUM(TestEnum, (Value1, _value__2, v3, ))

static_assert(enum_utils::count<TestEnum> == 3);

static_assert(enum_utils::to_string(TestEnum::Value1) == "Value1");
static_assert(enum_utils::to_string(TestEnum::_value__2) == "_value__2");
static_assert(enum_utils::to_string(TestEnum::v3) == "v3");
static_assert(enum_utils::to_string(TestEnum::v3) != "something_else");

static_assert(enum_utils::from_string<TestEnum>("Value1") == TestEnum::Value1);
static_assert(enum_utils::from_string<TestEnum>("_value__2") == TestEnum::_value__2);
static_assert(enum_utils::from_string<TestEnum>("v3") == TestEnum::v3);
static_assert(enum_utils::from_string<TestEnum>("value1") == nullopt);
static_assert(enum_utils::from_string<TestEnum>(" Value1") == nullopt);
static_assert(enum_utils::from_string<TestEnum>("Value1 ") == nullopt);
static_assert(enum_utils::from_string<TestEnum>("Value1,") == nullopt);

constexpr enum_utils::Map<TestEnum, int> test_map = {
    { TestEnum::Value1, 1 },
    { TestEnum::_value__2, 2 },
    { TestEnum::v3, 3 },
};

static_assert(test_map[TestEnum::Value1] == 1);
static_assert(test_map[TestEnum::_value__2] == 2);
static_assert(test_map[TestEnum::v3] == 3);

} // namespace Mg::test
