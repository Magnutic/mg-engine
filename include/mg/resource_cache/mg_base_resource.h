//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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

enum class LoadResourceResultCode { Success, DataError };

struct LoadResourceResult {
    static LoadResourceResult success() { return { LoadResourceResultCode::Success, "" }; }

    static LoadResourceResult data_error(std::string_view reason)
    {
        return { LoadResourceResultCode::DataError, std::string(reason) };
    }

    LoadResourceResultCode result_code;
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
    explicit BaseResource(Identifier id) noexcept : m_id(id) {}
    virtual ~BaseResource() = default;

    MG_MAKE_DEFAULT_MOVABLE(BaseResource);
    MG_MAKE_NON_COPYABLE(BaseResource);

    /** Load resource from binary file data. This is the interface through which Mg::ResourceCache
     * initializes resource types.
     */
    LoadResourceResult load_resource(ResourceLoadingInput& input);

    virtual bool should_reload_on_file_change() const = 0;

    /** Get identifier for the actual type of the resource. As a convention, it is recommended to
     * use the same name as the class.
     */
    virtual Identifier type_id() const = 0;

    /** Resource identifier (filename, if loaded from file). */
    Identifier resource_id() const noexcept { return m_id; }

protected:
    /** Subclasses should override this to implement their resource loading logic. */
    virtual LoadResourceResult load_resource_impl(ResourceLoadingInput& input) = 0;

private:
    Identifier m_id;
};

} // namespace Mg
