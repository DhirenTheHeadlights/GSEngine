//enet has to be included before other stuff, otherwise you will get linking errors
//don't forget to also link to it in the cmake (you have to uncomment the add_subdirectory line and also
//  add enet in the target_link_libraries list at the end
//#include <enet/enet.h>

// Standard Library Includes
#include <iostream>
#include <ctime>
#include <fstream>
#include <chrono>

// Third-Party Library Includes
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image/stb_image.h>
#include <stb_truetype/stb_truetype.h>
#include <raudio.h>

// Project-Specific Includes
#include "gl2d/gl2d.h"
#include "platformTools.h"
#include "platformInput.h"
#include "otherPlatformFunctions.h"
#include "gameLayer.h"
#include "errorReporting.h"

#define REMOVE_IMGUI 0

#if REMOVE_IMGUI == 0
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imguiThemes.h"
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#undef min
#undef max

bool currentFullScreen = 0;
bool fullScreen = 0;

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

    if ((action == GLFW_REPEAT || action == GLFW_PRESS) && key == GLFW_KEY_BACKSPACE) {
        platform::internal::addToTypedInput(8);
    }

    bool state = 0;

    if (action == GLFW_PRESS) {
        state = 1;
    }
    else if (action == GLFW_RELEASE) {
        state = 0;
    }
    else {
        return;
    }

    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        int index = key - GLFW_KEY_A;
        platform::internal::setButtonState(platform::Button::A + index, state);
    }
    else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        int index = key - GLFW_KEY_0;
        platform::internal::setButtonState(platform::Button::NR0 + index, state);
    }
    else {
        //special keys
        //GLFW_KEY_SPACE, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT

        if (key == GLFW_KEY_SPACE) {
            platform::internal::setButtonState(platform::Button::Space, state);
        }
        else if (key == GLFW_KEY_ENTER) {
            platform::internal::setButtonState(platform::Button::Enter, state);
        }
        else if (key == GLFW_KEY_ESCAPE) {
            platform::internal::setButtonState(platform::Button::Escape, state);
        }
        else if (key == GLFW_KEY_UP) {
            platform::internal::setButtonState(platform::Button::Up, state);
        }
        else if (key == GLFW_KEY_DOWN) {
            platform::internal::setButtonState(platform::Button::Down, state);
        }
        else if (key == GLFW_KEY_LEFT) {
            platform::internal::setButtonState(platform::Button::Left, state);
        }
        else if (key == GLFW_KEY_RIGHT) {
            platform::internal::setButtonState(platform::Button::Right, state);
        }
        else if (key == GLFW_KEY_LEFT_CONTROL) {
            platform::internal::setButtonState(platform::Button::LeftCtrl, state);
        }
        else if (key == GLFW_KEY_TAB) {
            platform::internal::setButtonState(platform::Button::Tab, state);
        }
        else if (key == GLFW_KEY_LEFT_SHIFT) {
            platform::internal::setButtonState(platform::Button::LeftShift, state);
        }
        else if (key == GLFW_KEY_LEFT_ALT) {
            platform::internal::setButtonState(platform::Button::LeftAlt, state);
        }

    }

};

void mouseCallback(GLFWwindow* window, int key, int action, int mods) {
    bool state = 0;

    if (action == GLFW_PRESS) {
        state = 1;
    }
    else if (action == GLFW_RELEASE) {
        state = 0;
    }
    else {
        return;
    }

    if (key == GLFW_MOUSE_BUTTON_LEFT) {
        platform::internal::setLeftMouseState(state);
    }
    else if (key == GLFW_MOUSE_BUTTON_RIGHT) {
        platform::internal::setRightMouseState(state);
    }

}

bool windowFocus = 1;

void windowFocusCallback(GLFWwindow* window, int focused) {
    if (focused) {
        windowFocus = 1;
    }
    else {
        windowFocus = 0;
        //if you not capture the release event when the window loses focus,
        //the buttons will stay pressed
        platform::internal::resetInputsToZero();
    }
}

void windowSizeCallback(GLFWwindow* window, int x, int y) {
    platform::internal::resetInputsToZero();
}

int mouseMovedFlag = 0;

void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    mouseMovedFlag = 1;
}

void characterCallback(GLFWwindow* window, unsigned int codepoint) {
    if (codepoint < 127) {
        platform::internal::addToTypedInput(codepoint);
    }
}

#pragma region platform functions

GLFWwindow* window = 0;

namespace platform {

    void setRelMousePosition(int x, int y) {
        glfwSetCursorPos(window, x, y);
    }

    bool isFullScreen() {
        return fullScreen;
    }

    void setFullScreen(bool f) {
        fullScreen = f;
    }

    glm::ivec2 getFrameBufferSize() {
        int x = 0; int y = 0;
        glfwGetFramebufferSize(window, &x, &y);
        return { x, y };
    }

    glm::ivec2 getRelMousePosition() {
        double x = 0, y = 0;
        glfwGetCursorPos(window, &x, &y);
        return { x, y };
    }

    glm::ivec2 getWindowSize() {
        int x = 0; int y = 0;
        glfwGetWindowSize(window, &x, &y);
        return { x, y };
    }

    void showMouse(bool show) {
        if (show) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        }
    }

    bool isFocused() {
        return windowFocus;
    }

    bool mouseMoved() {
        return mouseMovedFlag;
    }

    bool writeEntireFile(const char* name, void* buffer, size_t size) {
        std::ofstream f(name, std::ios::binary);

        if (!f.is_open()) {
            return 0;
        }

        f.write((char*)buffer, size);

        f.close();

        return 1;
    }

    bool readEntireFile(const char* name, void* buffer, size_t size) {
        std::ifstream f(name, std::ios::binary);

        if (!f.is_open()) {
            return 0;
        }

        f.read((char*)buffer, size);

        f.close();

        return 1;
    }

};
#pragma endregion

void setUpImgui() {
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    imguiThemes::embraceTheDarkness();

    ImGuiIO& io = ImGui::GetIO(); (void)io; // void cast prevents unused variable warning
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoTaskBarIcon = true;

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.Colors[ImGuiCol_WindowBg].w = 0.f;
        style.Colors[ImGuiCol_DockingEmptyBg].w = 0.f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}


void mainLoop(int w, int h) {
    auto stop = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        auto start = std::chrono::high_resolution_clock::now();

        float deltaTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(start - stop)).count() / std::pow(10, 9);
        stop = std::chrono::high_resolution_clock::now();

        float augmentedDeltaTime = deltaTime;
        if (augmentedDeltaTime > 0.01f) { augmentedDeltaTime = 0.01f; }

#if REMOVE_IMGUI == 0
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
#endif

        if (!gameLogic(augmentedDeltaTime)) {
            closeGame();
            return;
        }

        if (platform::isFocused() && currentFullScreen != fullScreen) {
            static int lastW = w;
            static int lastH = w;
            static int lastPosX = 0;
            static int lastPosY = 0;

            if (fullScreen) {
                lastW = w;
                lastH = h;

                //glfwWindowHint(GLFW_DECORATED, NULL); // Remove the border and titlebar..  
                glfwGetWindowPos(window, &lastPosX, &lastPosY);

                //auto monitor = glfwGetPrimaryMonitor();
                auto monitor = getCurrentMonitor(window);

                const GLFWvidmode* mode = glfwGetVideoMode(monitor);

                // switch to full screen
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

                currentFullScreen = 1;

            }
            else {
                //glfwWindowHint(GLFW_DECORATED, GLFW_TRUE); // 
                glfwSetWindowMonitor(window, nullptr, lastPosX, lastPosY, lastW, lastH, 0);

                currentFullScreen = 0;
            }

        }

        mouseMovedFlag = 0;
        platform::internal::updateAllButtons(deltaTime);
        platform::internal::resetTypedInput();

#if REMOVE_IMGUI == 0
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
#endif

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

int main() {

#ifdef _WIN32
#ifdef _MSC_VER 
#if PRODUCTION_BUILD == 0
    AllocConsole();
    (void)freopen("conin$", "r", stdin);
    (void)freopen("conout$", "w", stdout);
    (void)freopen("conout$", "w", stderr);
    std::cout.sync_with_stdio();
#endif
#endif
#endif

#pragma region window and opengl

    permaAssertComment(glfwInit(), "err initializing glfw");
    glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

    int w = 500;
    int h = 500;
    window = glfwCreateWindow(w, h, "geam", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseCallback);
    glfwSetWindowFocusCallback(window, windowFocusCallback);
    glfwSetWindowSizeCallback(window, windowSizeCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetCharCallback(window, characterCallback);

    //permaAssertComment(gladLoadGL(), "err initializing glad");
    permaAssertComment(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "err initializing glad");

    enableReportGlErrors();

#pragma endregion

    gl2d::init();

    #if REMOVE_IMGUI == 0
	    setUpImgui();
    #endif

    if (!initGame()) {
        return 0;
    }

    mainLoop(w, h);

    closeGame();

    // Keep the console open in debug mode
    std::cin.clear();
    std::cin.get();
}
