//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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
    Identifier resource_type;
    std::time_t time_stamp;
};
} // namespace Mg
