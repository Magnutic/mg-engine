//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
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

/** @file mg_assert.h
 * Assertion macros.
 */

#pragma once

#include <cstdio>
#include <cstdlib>

#ifndef MG_CONTRACT_VIOLATION_THROWS
#    define MG_CONTRACT_VIOLATION_THROWS 0
#endif

/** MG_ASSERT will check even in release builds -- use when checking for errors is critical, more
 * important than performance.
 */
#define MG_ASSERT(expr) \
    static_cast<void>((expr) || (::Mg::contract_violation(#expr, __FILE__, __LINE__), 0))

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
[[noreturn]] inline void contract_violation(const char* expr, const char* file, int line)
{
    fprintf(stderr, "Assertion failed: %s at %s:%d\n", expr, file, line);

#if MG_CONTRACT_VIOLATION_THROWS
    throw ContractViolation{};
#else

    std::abort();
#endif
}

} // namespace Mg
