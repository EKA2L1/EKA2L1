#include "imguirdr.h"
#include "window.h"

#include <iostream>

void gui_test() {
    ImGui::NewFrame();

    ImGui::Begin("Window hello");
    ImGui::Text("Hello world");
    ImGui::End();

    ImGui::Render();
}

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

    ImGui::StyleColorsDark();

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        eka2l1::imgui::newframe_gl(win);

        static float f = 0.0f;

        static int counter = 0;
        ImGui::Text("Hello, world!");                           // Display some text (you can use a format string too)
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our windows open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (NB: most widgets return true when edited/activated)
            counter++;

        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
        // 2. Show another simple window. In most cases you will use an explicit Begin/End pair to name your windows.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // 3. Show the ImGui demo window. Most of the sample code is in ImGui::ShowDemoWindow(). Read its code to learn more about Dear ImGui!
        if (show_demo_window)
        {
            ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver); // Normally user code doesn't need/want to call this because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
            ImGui::ShowDemoWindow(&show_demo_window);
        }

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
