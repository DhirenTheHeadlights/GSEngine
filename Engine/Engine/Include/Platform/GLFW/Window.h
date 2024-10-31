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

    GLFWwindow* getWindow();
    bool isWindowClosed();
    bool isFullScreen();
    bool isWindowFocused();
    bool isMouseVisible();
    int hasMouseMoved();

	void initialize();
	void update();
    void shutdown();

    GLFWmonitor* getCurrentMonitor();

    void setMousePosRelativeToWindow(const glm::ivec2& position);

    glm::ivec2 getFrameBufferSize();
    glm::ivec2 getRelMousePosition();
    glm::ivec2 getWindowSize();

    void setFullScreen(bool fullScreen);
    void setWindowFocused(bool focused);
    void setMouseVisible(bool show);

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouseCallback(GLFWwindow* window, int button, int action, int mods);
    void windowFocusCallback(GLFWwindow* window, int focused);
    void windowSizeCallback(GLFWwindow* window, int x, int y);
    void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    void characterCallback(GLFWwindow* window, unsigned int codepoint);
}
