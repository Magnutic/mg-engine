//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_runtime_error.h
 * Runtime error exception class.
 */

#pragma once

#include "mg/core/mg_log.h"

#include <fmt/core.h>

#include <string>
#include <string_view>

namespace Mg {

class RuntimeError {
public:
    template<typename... Ts>
    explicit RuntimeError(fmt::format_string<Ts...> message, Ts&&... format_args)
        : m_message(fmt::format(message, std::forward<Ts>(format_args)...))
    {
        log.error(m_message);
    }

    virtual ~RuntimeError() = default;

    virtual const char* what() const noexcept { return m_message.c_str(); }

private:
    std::string m_message;
};

} // namespace Mg
