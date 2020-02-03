//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_runtime_error.h
 * Runtime error exception class.
 */

#pragma once

#include <exception>

namespace Mg {

class RuntimeError : public std::exception {
public:
    const char* what() const noexcept override { return m_message; }

private:
    const char* m_message = "An unexpected error occurred; see log for details.";
};

} // namespace Mg
