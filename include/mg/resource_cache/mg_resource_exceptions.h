//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_resource_exceptions.h
 * Exception types related to the loading of resource data.
 */

#pragma once

#include "mg/core/mg_runtime_error.h"

namespace Mg {

/** Base of exceptions thrown on resource loading errors. */
class ResourceError : public RuntimeError {
    using RuntimeError::RuntimeError;
};

/** Exception thrown when a requested resource cannot be found. */
class ResourceNotFound : public ResourceError {
public:
    ResourceNotFound()
        : ResourceError("A requested resource file could not be found (see log for details)")
    {}
};

/** Exception thrown when a resource file had invalid data. */
class ResourceDataError : public ResourceError {
public:
    ResourceDataError()
        : ResourceError(
              "A requested resource file could not be loaded due to invalid data (see log for "
              "details)")
    {}
};

} // namespace Mg
