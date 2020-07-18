//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/audio/mg_sound_buffer_handle.h"

#include "mg/audio/mg_audio_context.h"

namespace Mg::audio {

void SoundBufferHandle::increment_ref_count()
{
    if (!m_ptr) return;
    AudioContext::get().increment_ref_count(m_ptr);
}

void SoundBufferHandle::decrement_ref_count()
{
    if (!m_ptr) return;
    AudioContext::get().decrement_ref_count(m_ptr);
}

} // namespace Mg::audio
