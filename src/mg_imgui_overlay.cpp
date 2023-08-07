//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/mg_imgui_overlay.h"

#include "mg/core/mg_window.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

#include "gfx/opengl/mg_glad.h" // TODO: temp

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <cmath>

namespace Mg {

ImguiOverlay::ImguiOverlay(const Window& window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Dear Imgui platform and renderer backends.
    const char* glsl_version = "#version 440 core";
    ImGui_ImplGlfw_InitForOpenGL(window.glfw_window(), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Make all colors a bit brighter.
    auto& colors = ImGui::GetStyle().Colors;
    for (auto& color : colors) {
        color.x = std::pow(color.x, 1 / 1.8f);
        color.y = std::pow(color.y, 1 / 1.8f);
        color.z = std::pow(color.z, 1 / 1.8f);
        color.w = std::pow(color.w, 1 / 1.8f);
    }
}

ImguiOverlay::~ImguiOverlay()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImguiOverlay::render(const std::function<void()>& imgui_function)
{
    // TODO: deal with the information in the following comment, copied from example code:
    //
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
    // wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
    // application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
    // application. Generally you may always pass all inputs to dear imgui, and hide them from
    // your application based on those two flags.

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    imgui_function();

    // Adjust framebuffer SRGB setting according to Dear Imgui's expectations.
    glDisable(GL_FRAMEBUFFER_SRGB);
    auto restore_srgb = finally([] { glEnable(GL_FRAMEBUFFER_SRGB); });

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
} // namespace Mg
