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

/** @file mg_resource_exceptions.h
 * Exception types related to the loading of resource data.
 */

#pragma once

#include <exception>

namespace Mg {

/** Base of exceptions thrown on resource loading errors. */
class ResourceError : public std::exception {};

/** Exception thrown when a requested resource cannot be found. */
class ResourceNotFound : public ResourceError {
public:
    ResourceNotFound() = default;

    const char* what() const noexcept override
    {
        return "A requested resource file could not be found (see log for details)";
    }
};

/** Exception thrown when a resource file had invalid data. */
class ResourceDataError : public ResourceError {
public:
    ResourceDataError() = default;

    const char* what() const noexcept override
    {
        return "A requested resource file could not be loaded due to invalid data (see log for "
               "details)";
    }
};

/** Exception thrown when a resource can not be loaded due to the ResourceCache being out of memory.
 */
class ResourceCacheOutOfMemory : public ResourceError {
public:
    ResourceCacheOutOfMemory() = default;

    const char* what() const noexcept override
    {
        return "A requested resource file could not be loaded due to the ResourceCache being out "
               "of memory (see log for details).";
    }
};

}
