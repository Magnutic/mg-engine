//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_value.h"

#include "mg/core/mg_log.h"
#include "mg/parser/mg_parser.h"
#include "mg/utils/mg_string_utils.h"
#include "mg/utils/mg_to_hjson.h"

#include <hjson/hjson.h>

namespace Mg {

Value::FromHjsonResult Value::from_hjson(const Hjson::Value& value)
{
    switch (value.type()) {
    case Hjson::Type::Bool:
        return { value.to_int64() != 0, "" };

    case Hjson::Type::Double:
        return { value.to_double(), "" };

    case Hjson::Type::Int64:
        return { value.to_int64(), "" };

    case Hjson::Type::Map:
        return { nullopt, "Cannot parse a Map ({}-enclosed) as a Value." };

    case Hjson::Type::Null:
        return { nullopt, "Value was null" };

    case Hjson::Type::String:
        return { nullopt, "Value was a string" };

    case Hjson::Type::Undefined:
        return { nullopt, "Value was undefined." };

    case Hjson::Type::Vector:
        // Range-based for loop is only valid for Map values in Hjson.
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (int i = 0; i < int(value.size()); ++i) {
            if (!value[i].is_numeric()) {
                return { nullopt, "Expected only numeric elements in vector." };
            }
        }

        switch (value.size()) {
        case 2:
            return { glm::vec2{ value[0].to_double(), value[1].to_double() }, "" };

        case 3:
            return { glm::vec3{ value[0].to_double(), value[1].to_double(), value[2].to_double() },
                     "" };

        case 4:
            return { glm::vec4{ value[0].to_double(),
                                value[1].to_double(),
                                value[2].to_double(),
                                value[3].to_double() },
                     "" };

        default:
            return { nullopt, "Expected a vector of 2, 3, or 4 elements." };
        }

    default:
        MG_ASSERT(false && "Unexpected error: invalid Hjson::Type");
    }
}

Value::Type Value::parse(std::string_view value_string)
{
    Hjson::Value value;
    try {
        value = Hjson::Unmarshal(value_string.data(), value_string.size());
    }
    catch (const Hjson::syntax_error& e) {
        throw RuntimeError{ "Failed to parse '{}' as a value.", value_string };
    }

    auto [result, error_reason] = from_hjson(value);
    if (!result) {
        throw RuntimeError{ "Failed to parse '{}' as a value: {}", value_string, error_reason };
    }

    m_data = result->m_data;
    return type();
}

Hjson::Value Value::to_hjson() const
{
    Hjson::Value hjson_value;
    std::visit([&](auto value) { hjson_value = ::Mg::to_hjson(value); }, m_data);
    return hjson_value;
}

std::string Value::serialize() const
{
    return Hjson::Marshal(to_hjson());
}

void Value::write_binary_data(span<std::byte> destination)
{
    std::visit(
        [&](const auto& value) {
            if (sizeof(value) > destination.size_bytes()) {
                throw RuntimeError{
                    "Value::write_binary_data: trying to write to too-small buffer."
                };
            }

            std::memcpy(destination.data(), &value, sizeof(value));
        },
        m_data);
}

} // namespace Mg
