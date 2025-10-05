//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/resources/mg_sound_resource.h"

#include "mg/core/audio/mg_audio_context.h"
#include "mg/core/resource_cache/mg_resource_loading_input.h"

namespace Mg {

LoadResourceResult SoundResource::load_resource_impl(ResourceLoadingInput& input)
{
    audio::AudioContext::GenerateSoundBufferResult result =
        audio::AudioContext::get().generate_sound_buffer(input.resource_data());

    if (!result.handle) {
        return LoadResourceResult::data_error(result.error_reason);
    }

    m_handle = std::move(result.handle);

    return LoadResourceResult::success();
}

} // namespace Mg
