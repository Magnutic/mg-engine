//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_resource_handle.h
 * Handle to a resource.
 * @see ResourceCache
 */

#pragma once

#include "mg/core/mg_identifier.h"

namespace Mg {

// Forward declarations.
class ResourceCache;
class ResourceEntryBase;
template<typename ResT> class ResourceEntry;
template<typename ResT> class ResourceAccessGuard;
template<typename ResT> class ResourceHandle;

/** ResourceHandle to unknown type of resource. */
class BaseResourceHandle {
public:
    BaseResourceHandle() = default;
    BaseResourceHandle(Identifier id, ResourceEntryBase& entry) noexcept
        : m_id(id), m_p_entry(&entry)
    {}

    Identifier resource_id() const noexcept { return m_id; }

protected:
    template<typename ResT> friend class ResourceAccessGuard;

    Identifier m_id = "";
    ResourceEntryBase* m_p_entry = nullptr;
};

/** Storable handle to a resource. */
template<typename ResT> class ResourceHandle : public BaseResourceHandle {
public:
    ResourceHandle() = default;
    ResourceHandle(Identifier id, ResourceEntry<ResT>& entry) noexcept
        : BaseResourceHandle(id, entry)
    {}
};

} // namespace Mg
