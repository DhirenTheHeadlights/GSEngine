#include <enet/enet.h> // Included first to avoid linking errors

// Standard Library Includes
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>

// Third-Party Library Includes
#include <raudio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image/stb_image.h>
#include <stb_truetype/stb_truetype.h>

// Project-Specific Includes
#include "CallBacks.h"
#include "ErrorReporting.h"
#include "GameLayer.h"
#include "PlatformFunctions.h"
#include "PlatformInput.h"
#include "PlatformTools.h"
#include "gl2d/gl2d.h"

#define REMOVE_IMGUI 0

#if REMOVE_IMGUI == 0
#include "imgui.h"
#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#undef min
#undef max

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

    ImGui_ImplGlfw_InitForOpenGL(Platform::window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}


void mainLoop(const int w, const int h) {
    auto stop = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(Platform::window)) {
        auto start = std::chrono::high_resolution_clock::now();

        const float deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(start - stop).count() / std::pow(10, 9);
        stop = std::chrono::high_resolution_clock::now();

        float augmentedDeltaTime = deltaTime;
        if (augmentedDeltaTime > 0.01f) { augmentedDeltaTime = 0.01f; }

#if REMOVE_IMGUI == 0
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
#endif

        if (!Game::gameLogic(augmentedDeltaTime)) {
            Game::closeGame();
            return;
        }

        if (Platform::windowFocused && Platform::currentFullScreen != Platform::fullScreen) {
            static int lastW = w;
            static int lastH = w;
            static int lastPosX = 0;
            static int lastPosY = 0;

            if (Platform::fullScreen) {
                lastW = w;
                lastH = h;

                //glfwWindowHint(GLFW_DECORATED, NULL); // Remove the border and titlebar..  
                glfwGetWindowPos(Platform::window, &lastPosX, &lastPosY);

                //auto monitor = glfwGetPrimaryMonitor();
                const auto monitor = Platform::getCurrentMonitor();

                const GLFWvidmode* mode = glfwGetVideoMode(monitor);

                // switch to full screen
                glfwSetWindowMonitor(Platform::window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

                Platform::currentFullScreen = 1;

            }
            else {
                //glfwWindowHint(GLFW_DECORATED, GLFW_TRUE); // 
                glfwSetWindowMonitor(Platform::window, nullptr, lastPosX, lastPosY, lastW, lastH, 0);

                Platform::currentFullScreen = 0;
            }

        }

        Platform::mouseMoved = 0;
        Platform::internal::updateAllButtons(deltaTime);
        Platform::internal::resetTypedInput();

#if REMOVE_IMGUI == 0
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(Platform::window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        const ImGuiIO& io = ImGui::GetIO(); (void)io;
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
#endif

        glfwSwapBuffers(Platform::window);
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

    int w = glfwGetVideoMode(glfwGetPrimaryMonitor())->width;
    int h = glfwGetVideoMode(glfwGetPrimaryMonitor())->height;
    Platform::window = glfwCreateWindow(w, h, "SavantShooter", nullptr, nullptr);
    glfwMakeContextCurrent(Platform::window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(Platform::window, Platform::keyCallback);
    glfwSetMouseButtonCallback(Platform::window, Platform::mouseCallback);
    glfwSetWindowFocusCallback(Platform::window, Platform::windowFocusCallback);
    glfwSetWindowSizeCallback(Platform::window, Platform::windowSizeCallback);
    glfwSetCursorPosCallback(Platform::window, Platform::cursorPositionCallback);
    glfwSetCharCallback(Platform::window, Platform::characterCallback);

    permaAssertComment(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "err initializing glad");

    enableReportGlErrors();

#pragma endregion

    gl2d::init();

    #if REMOVE_IMGUI == 0
	    setUpImgui();
    #endif

    if (!Game::initializeGame()) {
        return 0;
    }

    mainLoop(w, h);

    Game::closeGame();

    // Keep the console open in debug mode
    /*std::cin.clear();
    std::cin.get();*/
}
