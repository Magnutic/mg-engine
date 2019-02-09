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

    void init_resource(memory::DefragmentingAllocator& heap, std::string_view text)
    {
        auto begin = reinterpret_cast<const std::byte*>(text.data()); // NOLINT
        auto end   = begin + text.size();
        m_buffer   = heap.alloc_copy(begin, end);
    }
};

} // namespace Mg
