#include <mg/core/mg_log.h>
#include <mg/core/mg_window.h>
#include <mg/gfx/mg_gfx_device.h>
#include <mg/input/mg_input.h>

#include <mg/input/mg_keyboard.h>
#include <mg/input/mg_mouse.h>

#include <fmt/core.h>

void input_sample()
{
    using namespace Mg;

    auto window = Window::make({}, "Input");
    Mg::gfx::GfxDevice gfx_device{ *window };

    input::ButtonTracker button_tracker{ *window };
    input::MouseMovementTracker mouse_movement_tracker{ *window };

    auto bind_key = [&](const input::Key key) {
        auto command = Identifier::from_runtime_string(input::localized_key_name(key));
        button_tracker.bind(command, key);
    };

    bind_key(input::Key::Space);
    bind_key(input::Key::Apostrophe);
    bind_key(input::Key::Comma);
    bind_key(input::Key::Minus);
    bind_key(input::Key::Period);
    bind_key(input::Key::Slash);
    bind_key(input::Key::Num0);
    bind_key(input::Key::Num1);
    bind_key(input::Key::Num2);
    bind_key(input::Key::Num3);
    bind_key(input::Key::Num4);
    bind_key(input::Key::Num5);
    bind_key(input::Key::Num6);
    bind_key(input::Key::Num7);
    bind_key(input::Key::Num8);
    bind_key(input::Key::Num9);
    bind_key(input::Key::Semicolon);
    bind_key(input::Key::Equal);
    bind_key(input::Key::A);
    bind_key(input::Key::B);
    bind_key(input::Key::C);
    bind_key(input::Key::D);
    bind_key(input::Key::E);
    bind_key(input::Key::F);
    bind_key(input::Key::G);
    bind_key(input::Key::H);
    bind_key(input::Key::I);
    bind_key(input::Key::J);
    bind_key(input::Key::K);
    bind_key(input::Key::L);
    bind_key(input::Key::M);
    bind_key(input::Key::N);
    bind_key(input::Key::O);
    bind_key(input::Key::P);
    bind_key(input::Key::Q);
    bind_key(input::Key::R);
    bind_key(input::Key::S);
    bind_key(input::Key::T);
    bind_key(input::Key::U);
    bind_key(input::Key::V);
    bind_key(input::Key::W);
    bind_key(input::Key::X);
    bind_key(input::Key::Y);
    bind_key(input::Key::Z);
    bind_key(input::Key::LeftBracket);
    bind_key(input::Key::Backslash);
    bind_key(input::Key::RightBracket);
    bind_key(input::Key::GraveAccent);
    bind_key(input::Key::World1);
    bind_key(input::Key::World2);
    bind_key(input::Key::Enter);
    bind_key(input::Key::Tab);
    bind_key(input::Key::Backspace);
    bind_key(input::Key::Ins);
    bind_key(input::Key::Del);
    bind_key(input::Key::Right);
    bind_key(input::Key::Left);
    bind_key(input::Key::Down);
    bind_key(input::Key::Up);
    bind_key(input::Key::PageUp);
    bind_key(input::Key::PageDown);
    bind_key(input::Key::Home);
    bind_key(input::Key::End);
    bind_key(input::Key::CapsLock);
    bind_key(input::Key::ScrollLock);
    bind_key(input::Key::NumLock);
    bind_key(input::Key::PrintScreen);
    bind_key(input::Key::Pause);
    bind_key(input::Key::F1);
    bind_key(input::Key::F2);
    bind_key(input::Key::F3);
    bind_key(input::Key::F4);
    bind_key(input::Key::F5);
    bind_key(input::Key::F6);
    bind_key(input::Key::F7);
    bind_key(input::Key::F8);
    bind_key(input::Key::F9);
    bind_key(input::Key::F10);
    bind_key(input::Key::F11);
    bind_key(input::Key::F12);
    bind_key(input::Key::KP_0);
    bind_key(input::Key::KP_1);
    bind_key(input::Key::KP_2);
    bind_key(input::Key::KP_3);
    bind_key(input::Key::KP_4);
    bind_key(input::Key::KP_5);
    bind_key(input::Key::KP_6);
    bind_key(input::Key::KP_7);
    bind_key(input::Key::KP_8);
    bind_key(input::Key::KP_9);
    bind_key(input::Key::KP_decimal);
    bind_key(input::Key::KP_divide);
    bind_key(input::Key::KP_multiply);
    bind_key(input::Key::KP_subtract);
    bind_key(input::Key::KP_add);
    bind_key(input::Key::KP_enter);
    bind_key(input::Key::KP_equal);
    bind_key(input::Key::LeftShift);
    bind_key(input::Key::LeftControl);
    bind_key(input::Key::LeftAlt);
    bind_key(input::Key::LeftSuper);
    bind_key(input::Key::RightShift);
    bind_key(input::Key::RightControl);
    bind_key(input::Key::RightAlt);
    bind_key(input::Key::RightSuper);
    bind_key(input::Key::Menu);

    button_tracker.bind("quit", input::Key::Esc);

    button_tracker.bind("mouse1", input::MouseButton::left);
    button_tracker.bind("mouse2", input::MouseButton::right);
    button_tracker.bind("mouse3", input::MouseButton::middle);
    button_tracker.bind("mouse4", input::MouseButton::button4);
    button_tracker.bind("mouse5", input::MouseButton::button5);
    button_tracker.bind("mouse6", input::MouseButton::button6);
    button_tracker.bind("mouse7", input::MouseButton::button7);

    while (true) {
        window->poll_input_events();
        const auto events = button_tracker.get_button_events();
        const auto cursor_position = mouse_movement_tracker.mouse_cursor_position();
        const auto mouse_delta = mouse_movement_tracker.mouse_delta();

        if (mouse_delta != glm::vec2{ 0.0f, 0.0f }) {
            log.message(
                fmt::format("Cursor position: ({}, {})", cursor_position.x, cursor_position.y));
        }

        for (const auto& [id, state] : events) {
            if (state.was_pressed) {
                log.message(fmt::format("Pressed: {}", id.str_view()));
            }
            else if (state.was_released) {
                log.message(fmt::format("Released: {}", id.str_view()));
            }
        }

        if (events.find("quit")->second.was_pressed) {
            break;
        }

        gfx_device.clear(window->render_target);
        window->swap_buffers();
    }
}

int main()
{
    input_sample();
}
