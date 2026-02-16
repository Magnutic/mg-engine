//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_assert.h
 * Assertion macros.
 */

#pragma once

#include <cstdio>
#include <cstdlib>
#include <source_location>

#ifndef MG_CONTRACT_VIOLATION_THROWS
#    define MG_CONTRACT_VIOLATION_THROWS 0
#endif

/** MG_ASSERT will check even in release builds -- use when checking for errors is critical, more
 * important than performance.
 */
#define MG_ASSERT(expr)                                                           \
    if (expr) {                                                                   \
        [[likely]];                                                               \
    }                                                                             \
    else {                                                                        \
        ::Mg::contract_violation(#expr,                                           \
                                 std::source_location::current().function_name(), \
                                 std::source_location::current().file_name(),     \
                                 std::source_location::current().line());         \
    }

/** MG_ASSERT_DEBUG: debug-build assertion. */
#ifndef NDEBUG
#    define MG_ASSERT_DEBUG(expr) MG_ASSERT(expr)
#else
#    define MG_ASSERT_DEBUG(expr) static_cast<void>(0);
#endif

namespace Mg {

#if MG_CONTRACT_VIOLATION_THROWS
/** Exception thrown on contract violation if MG_CONTRACT_VIOLATION_THROWS is enabled.
 * Intended for unit-tests verifying that assertions are triggered.
 */
class ContractViolation {};
#endif

/** Function invoked on contract violation in e.g. MG_ASSERT */
[[noreturn]] inline void
contract_violation(const char* expr, const char* function, const char* file, int line)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    fprintf(stderr, "Assertion failed: '%s' in function '%s' at %s:%d\n", expr, function, file, line);

#if MG_CONTRACT_VIOLATION_THROWS
    throw ContractViolation{};
#else

    std::abort();
#endif
}

} // namespace Mg
