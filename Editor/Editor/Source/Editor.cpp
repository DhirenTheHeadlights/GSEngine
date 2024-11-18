#include "Editor.h"

#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace {
    GLuint fbo;                  // FBO ID
    GLuint fboTexture;           // Texture attached to the FBO
    GLuint depthBuffer;          // Depth buffer for the FBO
    int viewportWidth = 800;     // Width of the viewport
    int viewportHeight = 600;    // Height of the viewport
}

void Editor::initialize() {
	Engine::Debug::setImguiSaveFilePath(RESOURCES_PATH "imgui_state.json");
	Engine::Debug::setUpImGui();

    // Generate and bind the FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create a texture to attach to the FBO
    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportWidth, viewportHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

    // Create a depth buffer and attach it to the FBO
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, viewportWidth, viewportHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    glViewport(0, 0, viewportWidth, viewportHeight);

    Engine::sceneHandler.setFbo(fbo);
}

void Editor::bindFbo() {
 	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void Editor::unbindFbo() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, Engine::Window::getWindowSize().x, Engine::Window::getWindowSize().y);
}

void Editor::update() {
	Engine::Debug::updateImGui();
}

void Editor::render() {
    if (fbo != 0) {
        Engine::Debug::createWindow("Game", ImVec2(viewportWidth, viewportHeight), ImVec2(50, 50));

        ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(fboTexture)), ImVec2(viewportWidth, viewportHeight), ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();
    }

	Engine::Debug::renderImGui();
}

void Editor::exit() {
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &fboTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
}

