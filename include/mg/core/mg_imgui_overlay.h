//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_imgui_overlay.h
 * Dear Imgui overlay for Mg Engine.
 */

#pragma once

#include "mg/utils/mg_macros.h"

namespace Mg {

class Window;

/** If ImGui window wants to capture input from keyboard and/or mouse for itself. */
struct ImguiInputCapture {
    bool keyboard = false;
    bool mouse = false;
};

class ImguiOverlay {
public:
    explicit ImguiOverlay(const Window& window);

    ~ImguiOverlay();

    MG_MAKE_NON_COPYABLE(ImguiOverlay);
    MG_MAKE_NON_MOVABLE(ImguiOverlay);

    /** Call this before calling any ImGui functions. */
    void start_frame();

    /** Call this after calling all ImGui functions for the frame, but before render. */
    void finish_frame();

    /** Render the last finished frame. Note that we can render the same frame multiple times, so
     * this does not necessarily have to be done directly after finish_frame. That is useful for
     * decoupled time step (when simulation step runs at a fixed rate but rendering does not).
     */
    void render();

    /** Get whether the Imgui overlay wants to capture input (mouse, keyboard) due to focus on GUI
     * elements.
     */
    ImguiInputCapture input_capture() const { return m_input_capture; }

private:
    ImguiInputCapture m_input_capture;
};


} // namespace Mg
