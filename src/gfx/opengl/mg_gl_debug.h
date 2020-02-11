//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_gl_debug.h
 * OpenGL debugging utilities */

#pragma once

#include <cstddef>
#include <string_view>

/** Check for and notify on OpenGL errors. */
#define MG_CHECK_GL_ERROR()                                       \
    ::Mg::gfx::check_gl_error(static_cast<const char*>(__FILE__), \
                              static_cast<const char*>(__func__), \
                              __LINE__)

namespace Mg::gfx {

/** Get string stating error type for the given GL error code. */
const char* gl_error_string(uint32_t error_code) noexcept;

void check_gl_error(std::string_view file, std::string_view function, size_t line) noexcept;

//--------------------------------------------------------------------------------------------------
// KHR_debug extension utilities
//--------------------------------------------------------------------------------------------------

const char* source_string(uint32_t source) noexcept;

const char* type_string(uint32_t type) noexcept;

const char* severity_string(uint32_t severity) noexcept;

void ogl_error_callback(uint32_t source,
                        uint32_t type,
                        uint32_t id,
                        uint32_t severity,
                        int32_t /* length */,
                        const char* msg,
                        const void* /* user_param */
                        ) noexcept;

} // namespace Mg::gfx
