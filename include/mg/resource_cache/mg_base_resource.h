//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

#include <string>
#include <string_view>

namespace Mg {

class ResourceLoadingInput;

struct LoadResourceResult {
    enum ResultCode { Success, DataError };

    static LoadResourceResult success() { return { ResultCode::Success, "" }; }

    static LoadResourceResult data_error(std::string_view reason)
    {
        return { ResultCode::DataError, std::string(reason) };
    }

    ResultCode  result_code;
    std::string error_reason;
};

/** Resource interface. All resources for use with ResourceCache should derive from this.
 * Additionally, all subtypes should inherit BaseResource constructor (or provide a constructor with
 * the same signature).
 *
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
    LoadResourceResult load_resource(const ResourceLoadingInput& params);

    virtual bool should_reload_on_file_change() const = 0;

    /** Get identifier for the actual type of the resource. As a convention, it is recommended to
     * use the same name as the class.
     */
    virtual Identifier type_id() const = 0;

    /** Resource identifier (filename, if loaded from file). */
    Identifier resource_id() const { return m_id; }

protected:
    virtual LoadResourceResult load_resource_impl(const ResourceLoadingInput& params) = 0;

private:
    Identifier m_id;
};

} // namespace Mg
