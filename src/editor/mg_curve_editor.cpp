//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include <mg/core/mg_window.h>
#include <mg/editor/mg_curve_editor.h>
#include <mg/mg_imgui_overlay.h>
#include <mg/utils/mg_gsl.h>
#include <mg/utils/mg_math_utils.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_internal.h"

#include <fmt/core.h>

namespace Mg::editor {

namespace {

ImVec2 normalize(ImVec2 v)
{
    return v / std::sqrt(v.x * v.x + v.y * v.y);
}

// Convert position from 'curve-space' (x in [0.0f, 1.0f], y in [-1.0f, 1.0f]) to screen
// coordinates.
ImVec2 to_screenspace(ImRect bbox, ImVec2 position)
{
    const auto x = bbox.Min.x + (position.x * (bbox.Max.x - bbox.Min.x));
    const auto y = bbox.Min.y + ((0.5f - position.y * 0.5f) * (bbox.Max.y - bbox.Min.y));
    return ImVec2{ x, y };
};

// Inverse of the above.
ImVec2 from_screenspace(ImRect bbox, ImVec2 screen_position)
{
    const auto x = (screen_position.x - bbox.Min.x) / (bbox.Max.x - bbox.Min.x);
    const auto y = 1.0f - ((screen_position.y - bbox.Min.y) / (bbox.Max.y - bbox.Min.y)) * 2.0f;
    return { x, y };
};

struct ControlPointScreenPosition {
    ImVec2 point;
    ImVec2 right_tangent_point;
    ImVec2 left_tangent_point;
};

ControlPointScreenPosition control_point_screen_positions(ImRect bbox, const ControlPoint& cp)
{
    const auto tangent_handle_length_factor = 0.1f;
    const auto handle_length_pixels = tangent_handle_length_factor * (bbox.Max.x - bbox.Min.x);

    const auto point = to_screenspace(bbox, { cp.x, cp.y });

    // Calculate position of tangent handles.
    // It is not possible to draw these correctly, consistenly, and symmetrically, since the tangent
    // is specified in relation to the x-axis distance to the neighbouring control point, which may
    // differ from left to right. Therefore we cheat instead and assume the next control point is
    // 1.0f away. This provides a good enough visualization for intuitive purposes.
    // Note: tangents are negated from what might be expected, due to this working in screen space,
    // which has y-axis growing downwards.
    const auto left_tangent_handle_normalized = normalize(ImVec2{ -1.0f, cp.left_tangent });
    const auto right_tangent_handle_normalized = normalize(ImVec2{ 1.0f, -cp.right_tangent });

    const auto right_tangent_point = point + right_tangent_handle_normalized * handle_length_pixels;
    const auto left_tangent_point = point + left_tangent_handle_normalized * handle_length_pixels;

    ControlPointScreenPosition result;
    result.point = point;
    result.right_tangent_point = right_tangent_point;
    result.left_tangent_point = left_tangent_point;
    return result;
}

ImColor desaturate(ImColor colour)
{
    float h = 0.0f;
    float s = 0.0f;
    float v = 0.0f;
    ImGui::ColorConvertRGBtoHSV(colour.Value.x, colour.Value.y, colour.Value.z, h, s, v);
    s = 0.0f;
    ImGui::ColorConvertHSVtoRGB(h, s, v, colour.Value.x, colour.Value.y, colour.Value.z);
    return colour;
}

float tangent_from_mouse_position(ImRect bbox,
                                  const ControlPoint& control_point,
                                  ImVec2 mouse_position)
{
    const auto point = to_screenspace(bbox, { control_point.x, control_point.y });
    auto n = (point - mouse_position);
    n = n * (1.0f / n.x);

    if (!ImGui::GetIO().KeyShift) {
        n.y = std::clamp(std::round(n.y), -1.0f, 1.0f);
    }

    return -n.y; // Negative due to inverted y-axis in screen coords.
}

ImColor to_imcolor(const glm::ivec4& colour)
{
    return { colour.x, colour.y, colour.z, colour.w };
}

} // namespace

bool curve_editor_widget(const CurveEditorSettings& settings,
                         Curve& curve,
                         Opt<Curve::Index>& active_control_point_inout)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiIO io = ImGui::GetIO();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    MG_ASSERT(draw_list);
    MG_ASSERT(window);

    if (window->SkipItems) {
        return false;
    }

    bool changed = false;

    const auto available_space = ImGui::GetContentRegionAvail();
    ImVec2 canvas_size = { min(settings.desired_size.x, available_space.x),
                           min(settings.desired_size.y, available_space.y) };

    ImRect bbox = { window->DC.CursorPos, window->DC.CursorPos + canvas_size };
    ImGui::ItemSize(bbox);
    if (!ImGui::ItemAdd(bbox, 0)) {
        return false;
    }

    // Draw background.
    ImGui::RenderFrame(bbox.Min,
                       bbox.Max,
                       to_imcolor(settings.background_colour),
                       true,
                       style.FrameRounding);

    // Convert position from 'curve-space' (x in [0.0f, 1.0f], y in [-1.0f, 1.0f]) to screen
    // coordinates.
    auto to_screenspace = [&](ImVec2 position) {
        return Mg::editor::to_screenspace(bbox, position);
    };
    auto from_screenspace = [&](ImVec2 screen_position) {
        return Mg::editor::from_screenspace(bbox, screen_position);
    };

    // Draw background grid.
    {
        const int horizontal_grid_start = int(-1.0 / settings.grid_spacing_y);
        const int horizontal_grid_end = int(1.0 / settings.grid_spacing_y);

        for (int i = horizontal_grid_start; i <= horizontal_grid_end; ++i) {
            const float f = static_cast<float>(i) * settings.grid_spacing_y;
            const auto line_colour = to_imcolor(i == 0 ? settings.grid_central_x_axis_colour
                                                       : settings.grid_colour);
            draw_list->AddLine(to_screenspace({ 0.0f, f }),
                               to_screenspace({ 1.0f, f }),
                               line_colour);
        }

        const int vertical_grid_start = 0;
        const int vertical_grid_end = int(1.0f / settings.grid_spacing_x);

        for (int i = vertical_grid_start; i <= vertical_grid_end; ++i) {
            const float f = static_cast<float>(i) * settings.grid_spacing_x;
            const auto line_colour = to_imcolor(settings.grid_colour);

            draw_list->AddLine(to_screenspace({ f, -1.0f }),
                               to_screenspace({ f, 1.0f }),
                               line_colour);
        }
    }

    if (curve.control_points().empty()) {
        return changed;
    }

    // Draw curve.
    {
        auto x_prev = 0.0f;
        auto y_prev = curve.evaluate(x_prev);

        for (int i = 1; i < settings.num_curve_vertices; ++i) {
            const auto x = float(i - 1) / float(settings.num_curve_vertices - 1);
            const auto y = curve.evaluate(x);

            draw_list->AddLine(to_screenspace({ x_prev, y_prev }),
                               to_screenspace({ x, y }),
                               to_imcolor(settings.curve_colour),
                               settings.curve_width);

            x_prev = x;
            y_prev = y;
        }
    }

    const std::span control_points = curve.control_points();
    Opt<Curve::Index> hovered_control_point = nullopt;

    // Handle interaction
    {
        auto handle_button = [&](ImVec2 button_centre, const std::string& button_label) {
            auto restore_cursor_pos =
                finally([original_cursor_screen_pos = ImGui::GetCursorScreenPos()] {
                    ImGui::SetCursorScreenPos(original_cursor_screen_pos);
                });

            ImVec2 handle_half_size = { settings.handle_radius, settings.handle_radius };
            const auto button_position = button_centre - handle_half_size;
            ImGui::SetCursorScreenPos(button_position);
            ImGui::InvisibleButton(button_label.c_str(), handle_half_size * 2.0f);
        };

        bool interacted = false;

        for (size_t i = 0; i < control_points.size(); ++i) {
            const auto point = control_points[i];
            const auto screen_positions = control_point_screen_positions(bbox, point);

            handle_button(screen_positions.point, fmt::format("{}##handle_{}", settings.label, i));

            if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
                const auto s = fmt::format("CP {}: X: {:.2f}, Y: {:.2f}", i, point.x, point.y);
                ImGui::SetTooltip("%s", s.c_str());
            }

            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDown(0)) {
                    active_control_point_inout = i;
                }

                hovered_control_point = i;
            }

            if (active_control_point_inout == i) {
                const auto left_handle_button_id =
                    fmt::format("{}##left_handle_{}", settings.label, i);
                handle_button(screen_positions.left_tangent_point, left_handle_button_id);

                if (ImGui::IsItemActive()) {
                    interacted = true;
                }

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    const float tangent =
                        tangent_from_mouse_position(bbox, point, ImGui::GetMousePos());
                    curve.set_left_tangent(i, tangent);
                }

                if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
                    const auto s = fmt::format("Left tangent: {:.2f}", point.left_tangent);
                    ImGui::SetTooltip("%s", s.c_str());
                }

                const auto right_handle_button_id =
                    fmt::format("{}##right_handle_{}", settings.label, i);
                handle_button(screen_positions.right_tangent_point, right_handle_button_id);

                if (ImGui::IsItemActive()) {
                    interacted = true;
                }

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    const float tangent =
                        tangent_from_mouse_position(bbox, point, ImGui::GetMousePos());
                    curve.set_right_tangent(i, tangent);
                }

                if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
                    const auto s = fmt::format("Right tangent: {:.2f}", point.right_tangent);
                    ImGui::SetTooltip("%s", s.c_str());
                }
            }

            if (!interacted && active_control_point_inout == i && ImGui::IsMouseDragging(0)) {
                auto new_point = from_screenspace(ImGui::GetMousePos());

                // Always round to two decimals.
                new_point.x = std::round(new_point.x * 100.0f) / 100.0f;
                new_point.y = std::round(new_point.y * 100.0f) / 100.0f;

                // Round to grid unless shift is held.
                if (!io.KeyShift) {
                    new_point.y = std::round(new_point.y / settings.grid_spacing_y) *
                                  settings.grid_spacing_y;
                }

                curve.set_x(i, new_point.x);
                curve.set_y(i, new_point.y);
                interacted = true;
            }
        }

        if (io.MouseClicked[0] && !interacted) {
            active_control_point_inout = nullopt;
        }
    }

    // Draw control points.
    {
        auto draw_handle_circle = [&](ImVec2 position, ImColor colour, ImColor outline_colour) {
            draw_list->AddCircleFilled(position, settings.handle_radius, outline_colour);
            draw_list->AddCircleFilled(position,
                                       settings.handle_radius - settings.handle_outline_width,
                                       colour);
        };

        for (size_t i = 0; i < control_points.size(); ++i) {
            const auto screen_positions = control_point_screen_positions(bbox, control_points[i]);
            const bool is_active = active_control_point_inout &&
                                   active_control_point_inout.value() == i;
            const bool is_hovered = hovered_control_point && hovered_control_point.value() == i;
            const bool should_draw_tangent_handles = is_active || is_hovered;

            if (should_draw_tangent_handles) {
                auto desaturate_if_inactive = [is_active](glm::vec4 colour) {
                    return is_active ? to_imcolor(colour) : desaturate(to_imcolor(colour));
                };

                draw_list->AddLine(screen_positions.point,
                                   screen_positions.right_tangent_point,
                                   desaturate_if_inactive(settings.tangent_line_colour),
                                   settings.tangent_line_width);

                draw_list->AddLine(screen_positions.point,
                                   screen_positions.left_tangent_point,
                                   desaturate_if_inactive(settings.tangent_line_colour),
                                   settings.tangent_line_width);

                draw_handle_circle(screen_positions.left_tangent_point,
                                   desaturate_if_inactive(settings.tangent_handle_colour),
                                   desaturate_if_inactive(settings.tangent_handle_outline_colour));

                draw_handle_circle(screen_positions.right_tangent_point,
                                   desaturate_if_inactive(settings.tangent_handle_colour),
                                   desaturate_if_inactive(settings.tangent_handle_outline_colour));
            }

            const auto colour = to_imcolor(is_active ? settings.active_point_handle_colour
                                                     : settings.inactive_point_handle_colour);
            const auto outline_colour =
                to_imcolor(is_active ? settings.active_point_handle_outline_colour
                                     : settings.inactive_point_handle_outline_colour);

            draw_handle_circle(screen_positions.point, colour, outline_colour);
        }

        return changed;
    }
}

class CurveEditor::Impl {
public:
    explicit Impl(const Window& window, const CurveEditorSettings& settings)
        : m_gui(window), m_settings(settings)
    {}

    void update(Curve& curve)
    {
        auto update_impl = [&] {
            constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration;

            const auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize({ viewport->Size.x, viewport->Size.y });

            if (ImGui::Begin("Curve Editor", nullptr, window_flags)) {
                curve_editor_widget(m_settings, curve, m_active_control_point);
            }

            ImGui::End();
        };

        m_gui.render(update_impl);
    }

private:
    ImguiOverlay m_gui;
    Opt<Curve::Index> m_active_control_point = nullopt;
    CurveEditorSettings m_settings;
};

CurveEditor::CurveEditor(const Window& window, const CurveEditorSettings& settings)
    : m_impl(window, settings)
{}

void CurveEditor::update(Curve& curve)
{
    m_impl->update(curve);
}

} // namespace Mg::editor
