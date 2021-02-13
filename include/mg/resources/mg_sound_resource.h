//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_sound_resource.h
 * Sound clip resource file.
 */

#pragma once

#include "mg/audio/mg_sound_buffer_handle.h"
#include "mg/resource_cache/mg_base_resource.h"

namespace Mg {

class SoundResource : public BaseResource {
public:
    using BaseResource::BaseResource;

    bool should_reload_on_file_change() const noexcept override { return true; }

    Identifier type_id() const noexcept override { return "SoundResource"; }

    audio::SoundBufferHandle sound_buffer_handle() const noexcept { return m_handle; }

protected:
    LoadResourceResult load_resource_impl(ResourceLoadingInput& input) override;

private:
    audio::SoundBufferHandle m_handle;
};

} // namespace Mg
