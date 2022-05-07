//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_value.h"

#include "mg/core/mg_log.h"
#include "mg/parser/mg_parser.h"
#include "mg/utils/mg_string_utils.h"
#include "mg/utils/mg_vec_to_string.h"

namespace Mg {

Value::Type Value::parse(std::string_view value_string)
{
    bool success = false;
    std::visit([&](auto& value) { success = parser::parse(value_string, value); }, m_data);
    if (!success) {
        throw RuntimeError{ "Failed to parse '{}' as a value.", value_string };
    }

    return type();
}

std::string Value::serialize() const
{
    // Some types use std::to_string, other use Mg::to_string. The using statements allow referring
    // to both using unqualified calls to to_string.
    using Mg::to_string;
    using std::to_string;

    std::string result;
    std::visit([&](const auto& value) { result = to_string(value); }, m_data);

    return result;
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
