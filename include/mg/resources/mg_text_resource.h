//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_text_resource.h
 * Plain-text resource type.
 */

#pragma once

#include "mg/resources/mg_raw_resource.h"

namespace Mg {

/** Text file resource. */
class TextResource : public RawResource {
public:
    using RawResource::RawResource;

    std::string_view text() const
    {
        return std::string_view(reinterpret_cast<const char*>(bytes().data()), bytes().size());
    }

    Identifier type_id() const override { return "TextResource"; }
};

} // namespace Mg
