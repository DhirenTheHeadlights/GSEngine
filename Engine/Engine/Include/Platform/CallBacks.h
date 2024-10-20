#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Engine::Platform {
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    void mouseCallback(GLFWwindow* window, int button, int action, int mods);

    void windowFocusCallback(GLFWwindow* window, int focused);

    void windowSizeCallback(GLFWwindow* window, int x, int y);

    void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);

    void characterCallback(GLFWwindow* window, unsigned int codepoint);
}