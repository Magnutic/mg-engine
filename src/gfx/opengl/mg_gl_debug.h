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
