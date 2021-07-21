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

#include <imgui.h>

#include <functional>

namespace Mg {

class Window;

class ImguiOverlay {
public:
    explicit ImguiOverlay(const Window& window);

    ~ImguiOverlay();

    MG_MAKE_NON_COPYABLE(ImguiOverlay);
    MG_MAKE_NON_MOVABLE(ImguiOverlay);

    void render(const std::function<void()>& imgui_function);
};

} // namespace Mg
