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

#include <ctime>

namespace Mg {

/** Event sent by ResourceCache to notify whether a resource file has changed. */
struct FileChangedEvent {
    BaseResourceHandle resource;
    Identifier resource_type;
    std::time_t time_stamp;
};

class FileChangedTracker {
public:
    using CallbackT = void (*)(void* user_data, const FileChangedEvent&);

    explicit FileChangedTracker(CallbackT callback, void* user_data)
        : m_callback{ callback }, m_user_data{ user_data }
    {}

    void notify_update(const FileChangedEvent& event) { m_callback(m_user_data, event); }

private:
    CallbackT m_callback;
    void* m_user_data = nullptr;
};

} // namespace Mg
