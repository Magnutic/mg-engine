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

/** @file mg_file_changed_event.h
 * Event signifying that a resource file has been changed.
 */

#pragma once

#include "mg/resource_cache/mg_resource_handle.h"

#include <chrono>

namespace Mg {

using time_point = std::chrono::system_clock::time_point;

/** Event sent by ResourceCache to notify whether a resource file has changed.
 * @see ResourceCache::set_file_change_callback
 */
struct FileChangedEvent {
    BaseResourceHandle resource;
    Identifier         resource_type;
    time_point         time_stamp;
};
} // namespace Mg
