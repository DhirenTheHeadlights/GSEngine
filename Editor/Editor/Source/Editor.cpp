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
    int viewportWidth = 1920 / 2;     // Width of the viewport
    int viewportHeight = 1080 / 2;    // Height of the viewport

	ImVec2 gameWindowPosition = { 0, 0 };
	ImVec2 gameWindowSize = { static_cast<float>(viewportWidth), static_cast<float>(viewportHeight) };

    bool gameFocused = false;
}

void Editor::initialize() {
	gse::Debug::setImguiSaveFilePath(EDITOR_RESOURCES_PATH "imgui_state.ini");
	gse::Debug::setUpImGui();

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

    gse::Window::setFbo(fbo, { viewportWidth, viewportHeight });

    Game::setInputHandlingFlag(false); // Initialize to false
}

void Editor::bindFbo() {
 	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void Editor::unbindFbo() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gse::Window::getWindowSize().x, gse::Window::getWindowSize().y);
}

void Editor::update() {
	gse::Debug::updateImGui();

	if (ImGui::GetIO().KeysDown[ImGuiKey_Escape]) {
        gse::request_shutdown();
        exit();
	}

	const ImVec2 mousePosition = { static_cast<float>(gse::Window::getRelMousePosition().x), static_cast<float>(gse::Window::getRelMousePosition().y) };

	const bool mouseOverGameImage = mousePosition.x > gameWindowPosition.x && mousePosition.x < gameWindowPosition.x + gameWindowSize.x &&
									mousePosition.y > gameWindowPosition.y && mousePosition.y < gameWindowPosition.y + gameWindowSize.y;

    if (ImGui::GetIO().MouseClicked[0] && mouseOverGameImage) {
	    gameFocused = true;
    }

	if (gameFocused && ImGui::GetIO().KeysDown[ImGuiKey_Q]) {
		gameFocused = false;

        gse::Window::setMousePosRelativeToWindow({ static_cast<int>(gameWindowPosition.x + gameWindowSize.x / 2), static_cast<int>(gameWindowPosition.y + gameWindowSize.y / 2) });
    }

    Game::setInputHandlingFlag(gameFocused);
    gse::Window::setMouseVisible(!gameFocused);
}

void Editor::render() {
    ImGui::Begin("Game");

    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    // Determine if the aspect ratio is correct
    if (const float aspectRatio = availableSize.x / availableSize.y; std::fabs(aspectRatio - 16.0f / 9.0f) > 0.01f) {
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

		gse::Window::setFbo(fbo, { viewportWidth, viewportHeight });
    }

    const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(fboTexture)), ImVec2(static_cast<float>(viewportWidth), static_cast<float>(viewportHeight)), ImVec2(0, 1), ImVec2(1, 0));

    gameWindowPosition = imagePosition;
    gameWindowSize = ImVec2(static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));

    ImGui::End();

    ImGui::Begin("Editor");

    const auto scenes = gse::scene_handler.getAllScenes();
    const auto activeScenes = gse::scene_handler.getActiveScenes();
    const std::string currentSceneName = activeScenes.empty() ? "No Active Scene" : activeScenes[0]->getId()->tag;
    if (scenes.empty()) {
        ImGui::Text("No scenes available.");
    }
    else if (scenes.size() == 1) {
        ImGui::Text("Only one scene available: %s", scenes[0]->tag.c_str());
    }
    else {
        if (ImGui::BeginCombo("Select Scene", currentSceneName.c_str())) {
            for (const auto& scene : scenes) {
                const bool isSelected = !activeScenes.empty() && activeScenes[0]->getId()->tag == scene->tag;
                if (ImGui::Selectable(scene->tag.c_str(), isSelected)) {
                    if (!isSelected) { // Avoid redundant activation
                        gse::scene_handler.activateScene(scene);
                    }
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();

    ImGui::Begin("Editor2");
    ImGui::Text("Welcome to the Editor!");
    ImGui::Separator();
    ImGui::Text("Add your widgets for game objects, properties, and settings here.");
    ImGui::End();


    gse::Debug::renderImGui();
}

void Editor::exit() {
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &fboTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
}

