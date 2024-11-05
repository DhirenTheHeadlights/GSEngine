#include "Editor.h"

GLuint fbo;            // FBO ID
GLuint fboTexture;     // Texture attached to the FBO
GLuint depthBuffer;    // Depth buffer for the FBO
int viewportWidth = 800;     // Width of the viewport
int viewportHeight = 600;    // Height of the viewport

void Editor::bindFbo() {
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

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind the FBO
}

void Editor::unbindFbo() {
	glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &fboTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
}

void Editor::renderImGuiViewport() {
    ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(fboTexture)), ImVec2(static_cast<float>(viewportWidth), static_cast<float>(viewportHeight)));
}

void Editor::update() {
	
}

