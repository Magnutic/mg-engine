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

/** @file mg_base_resource.h
 * Base class for resource types.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_macros.h"

namespace Mg::memory {
class DefragmentingAllocator;
}

namespace Mg {

class LoadResourceParams;

/** Resource interface. All resources for use with ResourceCache should derive from this.
 * Additionally, all subtypes should inherit BaseResource constructor (or provide a constructor with
 * the same signature).
 *
 * @remark
 * When a resource type allocates memory, it should use the Mg::memory::DefragmentingAllocator that
 * is provided as a parameter to load_resource(), and store the resulting Mg::memory::DA_UniquePtr
 * buffer-handles. This keeps the resource's data stored within a dedicated memory buffer that can
 * be defragmented.
 *
 * @see Mg::memory::DefragmentingAllocator
 * @see Mg::ResourceHandle
 * @see Mg::ResourceCache
 */
class BaseResource {
public:
    explicit BaseResource(Identifier id) : m_id(id) {}

    MG_MAKE_DEFAULT_COPYABLE(BaseResource);
    MG_MAKE_DEFAULT_MOVABLE(BaseResource);

    virtual ~BaseResource() {}

    /** Load resource from binary file data. This is the interface through which Mg::ResourceCache
     * initialises resource types.
     */
    virtual void load_resource(const LoadResourceParams& params) = 0;

    virtual bool should_reload_on_file_change() const = 0;

    /** Resource identifier (filename, if loaded from file). */
    Identifier resource_id() const { return m_id; }

private:
    Identifier m_id;
};

} // namespace Mg
