//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_raw_resource.h
 * Raw (bytestream) resource type.
 */

#pragma once

#include "mg/core/containers/mg_array.h"
#include "mg/core/resource_cache/mg_base_resource.h"

#include <span>

namespace Mg {

class IFileLoader;

/** Raw (bytestream) resource. */
class RawResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    bool should_reload_on_file_change() const override { return true; }

    /** Access byte stream. */
    std::span<std::byte> bytes() noexcept { return m_buffer; }

    /** Access byte stream. */
    std::span<const std::byte> bytes() const noexcept { return m_buffer; }

    Identifier type_id() const override { return "RawResource"_id; }

protected:
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

    Array<std::byte> m_buffer;
};

} // namespace Mg
