#include "Editor.h"

#include "Game.h"
#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "ResourcePaths.h"

namespace {
    GLuint fbo;                  // FBO ID
    GLuint fboTexture;           // Texture attached to the FBO
    GLuint depthBuffer;          // Depth buffer for the FBO
    int viewportWidth = 800;     // Width of the viewport
    int viewportHeight = 600;    // Height of the viewport

	ImVec2 gameWindowPosition = { 0, 0 };
    ImVec2 gameWindowSize = { 1920 / 2, 1080 / 2 };

    bool gameFocused = false;
}

void Editor::initialize() {
	Engine::Debug::setImguiSaveFilePath(EDITOR_RESOURCES_PATH "imgui_state.ini");
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

    Engine::Window::setFbo(fbo, { viewportWidth, viewportHeight });

    Game::setInputHandlingFlag(false); // Initialize to false
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

	if (ImGui::GetIO().KeysDown[ImGuiKey_Escape]) {
        Engine::requestShutdown();
        exit();
	}

	const ImVec2 mousePosition = { static_cast<float>(Engine::Window::getRelMousePosition().x), static_cast<float>(Engine::Window::getRelMousePosition().y) };

	const bool mouseOverGameImage = mousePosition.x > gameWindowPosition.x && mousePosition.x < gameWindowPosition.x + gameWindowSize.x &&
									mousePosition.y > gameWindowPosition.y && mousePosition.y < gameWindowPosition.y + gameWindowSize.y;

    if (ImGui::GetIO().MouseClicked[0] && mouseOverGameImage) {
	    gameFocused = true;
    }

	if (gameFocused && ImGui::GetIO().KeysDown[ImGuiKey_Q]) {
		gameFocused = false;

        Engine::Window::setMousePosRelativeToWindow({ static_cast<int>(gameWindowPosition.x + gameWindowSize.x / 2), static_cast<int>(gameWindowPosition.y + gameWindowSize.y / 2) });
    }

    Game::setInputHandlingFlag(gameFocused);
    Engine::Window::setMouseVisible(!gameFocused);
}

void Editor::render() {
    ImGui::Begin("Game");

    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    // Determine if the aspect ratio is correct
    if (const float aspectRatio = availableSize.x / availableSize.y; std::fabs(aspectRatio - (16.0f / 9.0f)) > 0.01f) {
        // Figure out which direction is most economical to resize
        if (aspectRatio > 16.0f / 9.0f) {
            availableSize.x = availableSize.y * (16.0f / 9.0f);
        }
        else {
            availableSize.y = availableSize.x / (16.0f / 9.0f);
        }
    }

    if (static_cast<int>(availableSize.x) != viewportWidth || static_cast<int>(availableSize.y) != viewportHeight) {
        viewportWidth = static_cast<int>(availableSize.x);
        viewportHeight = static_cast<int>(availableSize.y);

        // Resize FBO
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Resize color texture
        glBindTexture(GL_TEXTURE_2D, fboTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportWidth, viewportHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

        // Resize depth buffer
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, viewportWidth, viewportHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

		Engine::Window::setFbo(fbo, { viewportWidth, viewportHeight });
    }

    const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(fboTexture)), ImVec2(static_cast<float>(viewportWidth), static_cast<float>(viewportHeight)), ImVec2(0, 1), ImVec2(1, 0));

    gameWindowPosition = imagePosition;
    gameWindowSize = ImVec2(static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));

    ImGui::End();

    ImGui::Begin("Editor");
    ImGui::Text("Welcome to the Editor!");
    ImGui::Separator();
    ImGui::Text("Add your widgets for game objects, properties, and settings here.");
    ImGui::End();

    ImGui::Begin("Editor2");
    ImGui::Text("Welcome to the Editor!");
    ImGui::Separator();
    ImGui::Text("Add your widgets for game objects, properties, and settings here.");
    ImGui::End();


    Engine::Debug::renderImGui();
}

void Editor::exit() {
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &fboTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
}

