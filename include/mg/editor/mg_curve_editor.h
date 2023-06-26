//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file editor/mg_curve_editor.h
 * Editor widget for editing Curve resources.
 */

#pragma once

#include <mg/core/mg_curve.h>
#include <mg/utils/mg_impl_ptr.h>
#include <mg/utils/mg_macros.h>
#include <mg/utils/mg_optional.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <string_view>

namespace Mg {
class Window;
}

/** Resource-editor widgets. */
namespace Mg::editor {

/** Settings for the `curve_editor_widget`. */
struct CurveEditorSettings {
    /** Label for the widget. */
    std::string_view label;

    /** Number of vertices used to visualize the curve. */
    int num_curve_vertices = 256;

    /** Desired size of the widget, in pixels. */
    glm::vec2 desired_size = { 500.0f, 200.0f };

    /** Width of the curve-line visualization. */
    float curve_width = 2.0f;

    /** Radius of control-point handles, in pixels. */
    float handle_radius = 6.0f;

    /** Radius of control-point handle outlines, in pixels. */
    float handle_outline_width = 2.0f;

    /** Width of the line connecting control-point handles with their tangent handles, in pixels. */
    float tangent_line_width = 2.0f;

    /** Spacing between vertical grid lines. */
    float grid_spacing_x = 0.2f;

    /** Spacing between horizontal grid lines. */
    float grid_spacing_y = 0.1f;

    glm::ivec4 background_colour = { 56, 56, 64, 255 };
    glm::ivec4 curve_colour = { 255, 255, 255, 255 };
    glm::ivec4 active_point_handle_colour = { 212, 132, 132, 170 };
    glm::ivec4 active_point_handle_outline_colour = { 212, 72, 72, 170 };
    glm::ivec4 inactive_point_handle_colour = { 132, 132, 212, 170 };
    glm::ivec4 inactive_point_handle_outline_colour = { 72, 72, 212, 170 };
    glm::ivec4 tangent_handle_colour = { 128, 176, 128, 255 };
    glm::ivec4 tangent_handle_outline_colour = { 32, 176, 32, 255 };
    glm::ivec4 tangent_line_colour = { 127, 204, 127, 127 };
    glm::ivec4 grid_colour = { 96, 96, 128, 255 };
    glm::ivec4 grid_central_x_axis_colour = { 128, 128, 160, 255 };
};

/** Draw an interactive curve-editor widget onto the current ImGui window. The widget allows the
 * user to edit the given curve object.
 * @param settings Settings for the widget.
 * @param curve The curve to edit.
 * @param active_control_point_inout In and out parameter for the index of the currently active
 * control point in the curve, if any. This state is changed as the user interacts with the control
 * points.
 * @return Whether any changes were made.
 */
bool curve_editor_widget(const CurveEditorSettings& settings,
                         Curve& curve,
                         Opt<Curve::Index>& active_control_point_inout);

/** A stand-alone curve editor that provides its own ImGui window. If you want to draw a curve
 * editor in an existing ImGui window, you can directly use `curve_editor_widget` instead.
 */
class CurveEditor {
public:
    explicit CurveEditor(const Window& window, const CurveEditorSettings& settings);

    void update(Curve& curve);

private:
    class Impl;
    ImplPtr<Impl> m_impl;
};

} // namespace Mg::editor
