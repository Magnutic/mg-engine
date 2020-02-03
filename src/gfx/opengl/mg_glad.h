//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_glad.h
 * Wrapper for including glad.h.
 * Defining APIENTRY prevents GLAD from including windows.h when building on Windows.
 */

#pragma once

#ifdef WIN32
#    ifndef APIENTRY
#        define APIENTRY __stdcall
#    endif
#endif

#include <glad/glad.h>
