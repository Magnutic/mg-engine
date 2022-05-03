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

namespace Mg {

// This macro defines the set of types available in a Value.
// Definition format: DO(enum_value, data_type, serialized_name)
#define MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST(DO, DO_LAST) \
    DO(Int, int, int)                                      \
    DO(Float, float, float)                                \
    DO(Vec2, glm::vec2, vec2)                              \
    DO(Vec3, glm::vec3, vec3)                              \
    DO_LAST(Vec4, glm::vec4, vec4)

#define MG_FOR_EACH_VALUE_TYPE(DO) MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST(DO, DO)

#define MG_VALUE_ENUM(enum_value, data_type, ...) enum_value,

#define MG_VALUE_VARIANT(_, data_type, ...) data_type,

#define MG_VALUE_VARIANT_LAST(_, data_type, ...) data_type

#define MG_VALUE_TO_STRING_MAP(enum_value, _, serialized_name) \
    std::pair<Type, std::string_view>{ Type::enum_value, #serialized_name },

#define MG_VALUE_CONSTRUCTOR(enum_value, data_type, ...) \
    Value(const data_type& value) : m_data(value) {}

#define MG_VALUE_GETTER(_, data_type, serialized_name)          \
    Opt<data_type> as_##serialized_name() const                 \
    {                                                           \
        if (auto* ptr = std::get_if<data_type>(&m_data); ptr) { \
            return *ptr;                                        \
        }                                                       \
        return nullopt;                                         \
    }

#define MG_VALUE_SETTER(enum_value, data_type, ...) \
    void set(const data_type& value) { m_data = value; }

/** Serializable Value holding a value of one alternative out of a set of dynamic types. */
class Value {
public:
    /** The dynamic type of the Value. */
    enum class Type { MG_FOR_EACH_VALUE_TYPE(MG_VALUE_ENUM) };

    /** Default constructor initializes as int value 0. */
    Value() : m_data(0) {}

    ~Value() = default;

    MG_MAKE_DEFAULT_MOVABLE(Value);
    MG_MAKE_DEFAULT_COPYABLE(Value);

    /** One constructor from each of the alternative value types. */
    MG_FOR_EACH_VALUE_TYPE(MG_VALUE_CONSTRUCTOR)

    /** Get the dynamic type of this Value. */
    Type type() const { return as<Type>(m_data.index()); }

    /** Get a string with the name of the dynamic type of this Value. */
    std::string type_as_string() const { return to_string(type()); }

    /** Try to get value as a particular type.
     * Returns nullopt if the requested type does not match the current dynamic type of this
     * Value.
     */
    MG_FOR_EACH_VALUE_TYPE(MG_VALUE_GETTER)

    /** Set value stored in this Value, possibly changing the dynamic type. */
    MG_FOR_EACH_VALUE_TYPE(MG_VALUE_SETTER)

    /** Parse the value_string and set this Value to match the results. Returns the new dynamic
     * type of this Value.
     */
    Type parse(std::string_view value_string);

    /** Serialize this Value to string. */
    std::string serialize() const;

    /** Write raw bytes to destination. Throws if destination is too small. */
    void write_binary_data(span<std::byte> destination);

    /** Convert Value::Type enum to string. */
    static std::string to_string(const Type& type)
    {
        for (const auto& [type_enum_value, type_string] : s_type_to_string) {
            if (type_enum_value == type) {
                return std::string(type_string);
            }
        }
        MG_ASSERT(false && "Unexpected Value::Type");
    }

private:
    // Data storage type
    using Data = std::variant<MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST(MG_VALUE_VARIANT,
                                                                    MG_VALUE_VARIANT_LAST)>;

    static_assert(std::is_trivially_copyable_v<Data>);

    // Mapping between enum and string representations.
    static constexpr std::array s_type_to_string = { //
        MG_FOR_EACH_VALUE_TYPE(MG_VALUE_TO_STRING_MAP)
    };

    Data m_data;
};

#undef MG_VALUE_CONSTRUCTOR
#undef MG_VALUE_GETTER
#undef MG_VALUE_SETTER
#undef MG_VALUE_VARIANT
#undef MG_VALUE_VARIANT_LAST
#undef MG_VALUE_ENUM
#undef MG_FOR_EACH_VALUE_TYPE_DIFFERENT_LAST
#undef MG_FOR_EACH_VALUE_TYPE

} // namespace Mg
