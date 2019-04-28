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
    uint32_t   option_flags;

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
