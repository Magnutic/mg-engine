//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file core/mg_value.h
 * Serializable values.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

// Forward declaration of the value type of the hjson parser.
namespace Hjson {
class Value;
}

namespace Mg {

// This macro defines the set of types available in a Value.
// Definition format: DO(enum_value, data_type, serialized_name)
#define MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST(DO, DO_LAST) \
    DO(Bool, bool, bool)                                   \
    DO(Int64, int64_t, int64)                              \
    DO(Double, double, double)                             \
    DO(Vec2, glm::vec2, vec2)                              \
    DO(Vec3, glm::vec3, vec3)                              \
    DO_LAST(Vec4, glm::vec4, vec4)

#define MG_FOR_EACH_VALUE_TYPE(DO) MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST(DO, DO)

namespace detail {

// Data storage type for Mg::Value.
using ValueVariant = std::variant<
#define MG_VALUE_VARIANT(_, data_type, ...) data_type,
#define MG_VALUE_VARIANT_LAST(_, data_type, ...) data_type

    MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST(MG_VALUE_VARIANT, MG_VALUE_VARIANT_LAST)

#undef MG_VALUE_VARIANT
#undef MG_VALUE_VARIANT_LAST
    >;


static_assert(std::is_trivially_copyable_v<detail::ValueVariant>);

template<typename data_type> Opt<data_type> get_as(const ValueVariant& v)
{
    if (auto* ptr = std::get_if<data_type>(&v); ptr) {
        return *ptr;
    }
    return nullopt;
}

template<> inline Opt<double> get_as<double>(const ValueVariant& v)
{
    // Allow widening from int to double.
    if (auto* ptr = std::get_if<int64_t>(&v); ptr) {
        return static_cast<double>(*ptr);
    }
    if (auto* ptr = std::get_if<double>(&v); ptr) {
        return *ptr;
    }
    return nullopt;
}

} // namespace detail

/** Serializable Value holding a value of one alternative out of a set of dynamic types. */
class Value {
public:
    /** The dynamic type of the Value. */
    enum class Type {
#define MG_VALUE_ENUM(enum_value, ...) enum_value,
        MG_FOR_EACH_VALUE_TYPE(MG_VALUE_ENUM)
#undef MG_VALUE_ENUM
    };

    struct FromHjsonResult;
    /** Convert from Hjson::Value to Mg::Value, if possible. */
    static FromHjsonResult from_hjson(const Hjson::Value& value);

    /** Convert from Mg::Value to Hjson::Value. */
    Hjson::Value to_hjson() const;

    /** Default constructor initializes as int value 0. */
    Value() : m_data(0) {}

    /** Construct a Value of the given type with a default value. */
    explicit Value(const Type type)
    {
        switch (type) {
#define MG_INIT_M_DATA_CASE(enum_value, data_type, ...) \
    case Type::enum_value:                              \
        m_data = data_type{};                           \
        break;

            MG_FOR_EACH_VALUE_TYPE(MG_INIT_M_DATA_CASE)

#undef MG_INIT_M_DATA_CASE
        }
        MG_ASSERT(false && "Unexpected Value::Type");
    }

    ~Value() = default;

    MG_MAKE_DEFAULT_MOVABLE(Value);
    MG_MAKE_DEFAULT_COPYABLE(Value);

#define MG_VALUE_CONSTRUCTOR(_, data_type, ...)                      \
    /** One constructor from each of the alternative value types. */ \
    Value(const data_type& value) : m_data(value) {}

    MG_FOR_EACH_VALUE_TYPE(MG_VALUE_CONSTRUCTOR)

#undef MG_VALUE_CONSTRUCTOR

    /** Get the dynamic type of this Value. */
    Type type() const { return as<Type>(m_data.index()); }

    /** Get a string with the name of the dynamic type of this Value. */
    std::string type_as_string() const { return type_to_string(type()); }

#define MG_VALUE_GETTER(_, data_type, serialized_name) \
    /** Try to get value as a particular type. */      \
    Opt<data_type> as_##serialized_name() const { return detail::get_as<data_type>(m_data); }

    MG_FOR_EACH_VALUE_TYPE(MG_VALUE_GETTER)

#undef MG_VALUE_GETTER

    /** Try to get value as float. */
    Opt<float> as_float() const
    {
        return as_double().map([&](auto v) { return narrow_cast<float>(v); });
    }

    /** Try to get value as int. */
    Opt<int> as_int() const
    {
        return as_int64().map([&](auto v) { return narrow_cast<int>(v); });
    }

#define MG_VALUE_SETTER(enum_value, data_type, ...)                            \
    /** Set value stored in this Value, possibly changing the dynamic type. */ \
    void set(const data_type& value) { m_data = value; }

    MG_FOR_EACH_VALUE_TYPE(MG_VALUE_SETTER)

#undef MG_VALUE_SETTER

    /** Parse the value_string and set this Value to match the results. Returns the new dynamic
     * type of this Value.
     */
    Type parse(std::string_view value_string);

    /** Serialize this Value to string. */
    std::string serialize() const;

    /** Write raw bytes to destination. Throws if destination is too small. */
    void write_binary_data(span<std::byte> destination);

    /** Convert Value::Type enum to string. */
    static std::string type_to_string(const Type& type)
    {
        for (const auto& [type_enum_value, type_string] : s_type_to_string) {
            if (type_enum_value == type) {
                return std::string(type_string);
            }
        }
        MG_ASSERT(false && "Unexpected Value::Type");
    }

    /** Convert string to Value::Type enum. */
    static Opt<Type> string_to_type(std::string_view string)
    {
        for (const auto& [type_enum_value, type_string] : s_type_to_string) {
            if (type_string == string) {
                return type_enum_value;
            }
        }
        return nullopt;
    }

private:
    // Mapping between enum and string representations.
    static constexpr std::array s_type_to_string = {
#define MG_VALUE_TO_STRING_MAP(enum_value, _, serialized_name) \
    std::pair<Type, std::string_view>{ Type::enum_value, #serialized_name },

        MG_FOR_EACH_VALUE_TYPE(MG_VALUE_TO_STRING_MAP)

#undef MG_VALUE_TO_STRING_MAP
    };

    detail::ValueVariant m_data;
};

struct Value::FromHjsonResult {
    Opt<Value> value;
    std::string error_reason;
};

#undef MG_FOR_EACH_VALUE_TYPE
#undef MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST

} // namespace Mg
