#include "imguirdr.h"
#include "window.h"
#include "io/imguiio.h"

#include <iostream>

static void glfw_err_callback(int err, const char* des){
    std::cout << err << ": " << des << std::endl;
}

int main() {
    glfwSetErrorCallback(glfw_err_callback);

    bool res = eka2l1::imgui::init();

    if (!res) {
        return 0;
    }

    GLFWwindow* win = eka2l1::imgui::open_window("EKA2L1", 800, 600);
    eka2l1::imgui::setup_io(win);

    ImGui::StyleColorsDark();

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        eka2l1::imgui::update_io(win);
        eka2l1::imgui::newframe_gl(win);

        static float f = 0.0f;

        static int counter = 0;

        ImGui::Begin("EKA2L1 Rom Information", &show_demo_window, ImVec2(1000, 1000));

        char* a = "0x2502";

        ImGui::InputText("UID", a, strlen(a), ImGuiInputTextFlags_ReadOnly);                           // Display some text (you can use a format string too)
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our windows open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (NB: most widgets return true when edited/activated)
            counter++;

        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::End();

        int display_w, display_h;
        glfwGetFramebufferSize(win, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        eka2l1::imgui::draw_gl(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    eka2l1::imgui::destroy_window(win);
    glfwTerminate();
}
