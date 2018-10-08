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

/** @file mg_resource_handle_fwd.h
 * Forward declaration of ResourceHandle.
 * Full implementation is in mg_resource_cache.h
 */

#pragma once

#include <mg/core/mg_identifier.h>

namespace Mg {

// Forward declarations.
class ResourceCache;
template<typename ResT> class ResourceAccessGuard;

/** Storable handle to a resource. */
template<typename ResT> class ResourceHandle {
public:
    explicit ResourceHandle(Identifier resource_id, ResourceCache& owning_cache)
        : m_resource_id(resource_id), m_owning_cache(&owning_cache)
    {}

    ResourceAccessGuard<ResT> access();

    Identifier resource_id() const noexcept { return m_resource_id; }

private:
    Identifier     m_resource_id;
    ResourceCache* m_owning_cache;
};

} // namespace Mg
