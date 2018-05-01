#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 1
#endif

#include <GLFW/glfw3native.h>

#include "imguiio.h"
#include <imgui.h>

namespace eka2l1 {
    namespace imgui {
        bool mouse_pressed[10];
        GLFWcursor *mouse_cursors[10];

        void on_scroll_back(GLFWwindow *, double x_off, double y_off) {
            ImGuiIO &io = ImGui::GetIO();

            io.MouseWheelH += (float)x_off;
            io.MouseWheel += (float)y_off;
        }

        void on_key_pressed(GLFWwindow *, int key, int, int action, int mods) {
            ImGuiIO &io = ImGui::GetIO();
            if (action == GLFW_PRESS)
                io.KeysDown[key] = true;
            if (action == GLFW_RELEASE)
                io.KeysDown[key] = false;

            (void)mods; // Modifiers are not reliable across systems
            io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
            io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
            io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
            io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
        }

        void char_callback(GLFWwindow *, unsigned int c) {
            ImGuiIO &io = ImGui::GetIO();
            if (c > 0 && c < 0x10000)
                io.AddInputCharacter((unsigned short)c);
        }

        void on_mouse_button_pressed(GLFWwindow *, int button, int action, int) {
            if (action == GLFW_PRESS && button >= 0 && button < 3)
                mouse_pressed[button] = true;
        }

        void install_callback(GLFWwindow *win) {
            glfwSetCharCallback(win, char_callback);
            glfwSetScrollCallback(win, on_scroll_back);
            glfwSetKeyCallback(win, on_key_pressed);
            glfwSetMouseButtonCallback(win, on_mouse_button_pressed);
        }

        const char *get_clipboard_text(void *user_data) {
            return glfwGetClipboardString((GLFWwindow *)user_data);
        }

        void set_clipboard_text(void *user_data, const char *text) {
            glfwSetClipboardString((GLFWwindow *)user_data, text);
        }

        void setup_io(GLFWwindow *win) {
            ImGuiIO &io = ImGui::GetIO();
            io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
            io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos; // We can honor io.WantSetMousePos requests (optional, rarely used)

            // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
            io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
            io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
            io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
            io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
            io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
            io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
            io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
            io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
            io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
            io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
            io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
            io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
            io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
            io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
            io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
            io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
            io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
            io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
            io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
            io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
            io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

            io.SetClipboardTextFn = set_clipboard_text;
            io.GetClipboardTextFn = get_clipboard_text;
            io.ClipboardUserData = win;
#ifdef _WIN32
            io.ImeWindowHandle = glfwGetWin32Window(win);
#endif

            mouse_cursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
            mouse_cursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
            mouse_cursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
            mouse_cursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
            mouse_cursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
            mouse_cursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
            mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

            install_callback(win);
        }

        void update_io(GLFWwindow *win) {
            ImGuiIO &io = ImGui::GetIO();

            if (glfwGetWindowAttrib(win, GLFW_FOCUSED)) {
                // Set OS mouse position if requested (only used when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
                if (io.WantSetMousePos) {
                    glfwSetCursorPos(win, (double)io.MousePos.x, (double)io.MousePos.y);
                } else {
                    double mouse_x, mouse_y;
                    glfwGetCursorPos(win, &mouse_x, &mouse_y);
                    io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
                }
            } else {
                io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            }

            for (int i = 0; i < 3; i++) {
                // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
                io.MouseDown[i] = mouse_pressed[i] || glfwGetMouseButton(win, i) != 0;
                mouse_pressed[i] = false;
            }

            // Update OS/hardware mouse cursor if imgui isn't drawing a software cursor
            if ((io.ConfigFlags & ImGuiConfigFlags_NoSetMouseCursor) == 0 && glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
                ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
                if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None) {
                    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                } else {
                    glfwSetCursor(win, mouse_cursors[cursor] ? mouse_cursors[cursor] : mouse_cursors[ImGuiMouseCursor_Arrow]);
                    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
            }

            // Gamepad navigation mapping [BETA]
            memset(io.NavInputs, 0, sizeof(io.NavInputs));
            if (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) {
// Update gamepad inputs
#define MAP_BUTTON(NAV_NO, BUTTON_NO)                                      \
    {                                                                      \
        if (buttons_count > BUTTON_NO && buttons[BUTTON_NO] == GLFW_PRESS) \
            io.NavInputs[NAV_NO] = 1.0f;                                   \
    }
#define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1)                    \
    {                                                          \
        float v = (axes_count > AXIS_NO) ? axes[AXIS_NO] : V0; \
        v = (v - V0) / (V1 - V0);                              \
        if (v > 1.0f)                                          \
            v = 1.0f;                                          \
        if (io.NavInputs[NAV_NO] < v)                          \
            io.NavInputs[NAV_NO] = v;                          \
    }
                int axes_count = 0, buttons_count = 0;
                const float *axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
                const unsigned char *buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
                MAP_BUTTON(ImGuiNavInput_Activate, 0); // Cross / A
                MAP_BUTTON(ImGuiNavInput_Cancel, 1); // Circle / B
                MAP_BUTTON(ImGuiNavInput_Menu, 2); // Square / X
                MAP_BUTTON(ImGuiNavInput_Input, 3); // Triangle / Y
                MAP_BUTTON(ImGuiNavInput_DpadLeft, 13); // D-Pad Left
                MAP_BUTTON(ImGuiNavInput_DpadRight, 11); // D-Pad Right
                MAP_BUTTON(ImGuiNavInput_DpadUp, 10); // D-Pad Up
                MAP_BUTTON(ImGuiNavInput_DpadDown, 12); // D-Pad Down
                MAP_BUTTON(ImGuiNavInput_FocusPrev, 4); // L1 / LB
                MAP_BUTTON(ImGuiNavInput_FocusNext, 5); // R1 / RB
                MAP_BUTTON(ImGuiNavInput_TweakSlow, 4); // L1 / LB
                MAP_BUTTON(ImGuiNavInput_TweakFast, 5); // R1 / RB
                MAP_ANALOG(ImGuiNavInput_LStickLeft, 0, -0.3f, -0.9f);
                MAP_ANALOG(ImGuiNavInput_LStickRight, 0, +0.3f, +0.9f);
                MAP_ANALOG(ImGuiNavInput_LStickUp, 1, +0.3f, +0.9f);
                MAP_ANALOG(ImGuiNavInput_LStickDown, 1, -0.3f, -0.9f);
#undef MAP_BUTTON
#undef MAP_ANALOG
                if (axes_count > 0 && buttons_count > 0)
                    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
                else
                    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
            }
        }
    }
}
