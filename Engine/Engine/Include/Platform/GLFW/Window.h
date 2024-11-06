#pragma once

#include <memory>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"

namespace Engine::Window {
    struct RenderingInterface {
        virtual ~RenderingInterface() = default;
        virtual void onPreRender() = 0;
        virtual void onPostRender() = 0;
    };

    void addRenderingInterface(const std::shared_ptr<RenderingInterface>& renderingInterface);
    void removeRenderingInterface(const std::shared_ptr<RenderingInterface>& renderingInterface);

    void initialize();
    void beginFrame();
    void update();
    void endFrame();
    void shutdown();

    GLFWwindow* getWindow();

    bool isWindowClosed();
    bool isFullScreen();
    bool isWindowFocused();
    bool isMouseVisible();
    int hasMouseMoved();

    GLFWmonitor* getCurrentMonitor();

    glm::ivec2 getFrameBufferSize();
    glm::ivec2 getRelMousePosition();
    glm::ivec2 getWindowSize();

    void setMousePosRelativeToWindow(const glm::ivec2& position);
    void setFullScreen(bool fullScreen);
    void setWindowFocused(bool focused);
    void setMouseVisible(bool show);

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouseCallback(GLFWwindow* window, int button, int action, int mods);
    void windowFocusCallback(GLFWwindow* window, int focused);
    void windowSizeCallback(GLFWwindow* window, int x, int y);
    void cursorPositionCallback(GLFWwindow* window, double x, double y);
    void characterCallback(GLFWwindow* window, unsigned int codepoint);
}
