#pragma once

#include <memory>
#include <optional>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"

namespace gse::Window {
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
    bool isFocused();
	bool isMinimized();
    bool isMouseVisible();
    int hasMouseMoved();

    GLFWmonitor* getCurrentMonitor();

    std::optional<GLuint> getFbo();
    glm::ivec2 getFrameBufferSize();
    glm::ivec2 getRelMousePosition();
    glm::ivec2 getWindowSize();
    glm::ivec2 getViewportSize();

    void setFbo(GLuint fboIn, const glm::ivec2& size);
    void setMousePosRelativeToWindow(const glm::ivec2& position);
    void setFullScreen(bool fs);
    void setWindowFocused(bool focused);
    void setMouseVisible(bool show);

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouseCallback(GLFWwindow* window, int button, int action, int mods);
    void windowFocusCallback(GLFWwindow* window, int focused);
    void windowSizeCallback(GLFWwindow* window, int x, int y);
    void cursorPositionCallback(GLFWwindow* window, double x, double y);
    void characterCallback(GLFWwindow* window, unsigned int codepoint);
}
