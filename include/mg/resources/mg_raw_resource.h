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

/** @file mg_raw_resource.h
 * Raw (bytestream) resource type.
 */

#pragma once

#include "mg/memory/mg_defragmenting_allocator.h"
#include "mg/resources/mg_base_resource.h"
#include "mg/utils/mg_gsl.h"

namespace Mg {

class IFileLoader;

/** Raw (bytestream) resource. */
class RawResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    bool should_reload_on_file_change() const override { return true; }

    /** Load from file. */
    /** Access byte stream. */
    span<std::byte> bytes() { return span{ m_buffer.begin(), m_buffer.end() }; }

    /** Access byte stream. */
    span<const std::byte> bytes() const { return span{ m_buffer.begin(), m_buffer.end() }; }

protected:
    LoadResourceResult load_resource_impl(const LoadResourceParams& load_params) override;

    memory::DA_UniquePtr<std::byte[]> m_buffer;
};

} // namespace Mg
