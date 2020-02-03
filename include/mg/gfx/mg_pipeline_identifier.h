//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_pipeline_identifier.h
 * Type which identifies which pipeline is to be used for a given Material.
 */

#pragma once

#include "mg/core/mg_identifier.h"

#include <cstdint>

namespace Mg::gfx {

/** Identifies which Pipeline is to be used for a Material. */
struct PipelineIdentifier {
    Identifier shader_resource_id;
    uint32_t option_flags;

    friend bool operator==(const PipelineIdentifier& lhs, const PipelineIdentifier& rhs)
    {
        return lhs.shader_resource_id == rhs.shader_resource_id &&
               lhs.option_flags == rhs.option_flags;
    }

    friend bool operator!=(const PipelineIdentifier& lhs, const PipelineIdentifier& rhs)
    {
        return !(lhs == rhs);
    }
};

} // namespace Mg::gfx
