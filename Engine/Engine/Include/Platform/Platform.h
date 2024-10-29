#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"

namespace Engine::Platform {
    extern GLFWwindow* window;

    extern bool currentFullScreen;
    extern bool fullScreen;
    extern bool windowFocused;
    extern bool mouseVisible;
    extern int mouseMoved;

	void initialize();

	void update();

    GLFWmonitor* getCurrentMonitor();

    void setMousePosRelativeToWindow(int x, int y);

    glm::ivec2 getFrameBufferSize();

    glm::ivec2 getRelMousePosition();

    glm::ivec2 getWindowSize();

    void showMouse(bool show);

    bool writeEntireFile(const char* name, void* buffer, size_t size);

    bool readEntireFile(const char* name, void* buffer, size_t size);
}
