#include "Editor.h"

#include "Game.h"
#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "ResourcePaths.h"

namespace {
    GLuint g_fbo;                  // FBO ID
    GLuint g_fbo_texture;           // Texture attached to the FBO
    GLuint g_depth_buffer;          // Depth buffer for the FBO
    int g_viewport_width = 1920 / 2;     // Width of the viewport
    int g_viewport_height = 1080 / 2;    // Height of the viewport

	ImVec2 g_game_window_position = { 0, 0 };
	ImVec2 g_game_window_size = { static_cast<float>(g_viewport_width), static_cast<float>(g_viewport_height) };

    bool g_game_focused = false;
}

void editor::initialize() {
	gse::debug::set_imgui_save_file_path(EDITOR_RESOURCES_PATH "imgui_state.ini");
	gse::debug::set_up_imgui();

    // Generate and bind the FBO
    glGenFramebuffers(1, &g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);

    // Create a texture to attach to the FBO
    glGenTextures(1, &g_fbo_texture);
    glBindTexture(GL_TEXTURE_2D, g_fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_viewport_width, g_viewport_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_fbo_texture, 0);

    // Create a depth buffer and attach it to the FBO
    glGenRenderbuffers(1, &g_depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, g_depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, g_viewport_width, g_viewport_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_depth_buffer);

    glViewport(0, 0, g_viewport_width, g_viewport_height);

    gse::window::set_fbo(g_fbo, { g_viewport_width, g_viewport_height });

    game::set_input_handling_flag(false); // Initialize to false
}

void editor::bind_fbo() {
 	glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glViewport(0, 0, g_viewport_width, g_viewport_height);
}

void editor::unbind_fbo() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gse::window::get_window_size().x, gse::window::get_window_size().y);
}

void editor::update() {
	gse::debug::update_imgui();

	if (ImGui::GetIO().KeysDown[ImGuiKey_Escape]) {
        gse::request_shutdown();
        exit();
	}

	const ImVec2 mouse_position = { static_cast<float>(gse::window::get_rel_mouse_position().x), static_cast<float>(gse::window::get_rel_mouse_position().y) };

	const bool mouse_over_game_image = mouse_position.x > g_game_window_position.x && mouse_position.x < g_game_window_position.x + g_game_window_size.x &&
									   mouse_position.y > g_game_window_position.y && mouse_position.y < g_game_window_position.y + g_game_window_size.y;

    if (ImGui::GetIO().MouseClicked[0] && mouse_over_game_image) {
	    g_game_focused = true;
    }

	if (g_game_focused && ImGui::GetIO().KeysDown[ImGuiKey_Q]) {
		g_game_focused = false;

        gse::window::set_mouse_pos_relative_to_window({ static_cast<int>(g_game_window_position.x + g_game_window_size.x / 2), static_cast<int>(g_game_window_position.y + g_game_window_size.y / 2) });
    }

    game::set_input_handling_flag(g_game_focused);
    gse::window::set_mouse_visible(!g_game_focused);
}

void editor::render() {
    ImGui::Begin("Game");

    ImVec2 available_size = ImGui::GetContentRegionAvail();
    // Determine if the aspect ratio is correct
    if (const float aspect_ratio = available_size.x / available_size.y; std::fabs(aspect_ratio - 16.0f / 9.0f) > 0.01f) {
        // Figure out which direction is most economical to resize
        if (aspect_ratio > 16.0f / 9.0f) {
            available_size.x = available_size.y * (16.0f / 9.0f);
        }
        else {
            available_size.y = available_size.x / (16.0f / 9.0f);
        }
    }

    if (static_cast<int>(available_size.x) != g_viewport_width || static_cast<int>(available_size.y) != g_viewport_height) {
        g_viewport_width = static_cast<int>(available_size.x);
        g_viewport_height = static_cast<int>(available_size.y);

        // Resize FBO
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);

        // Resize color texture
        glBindTexture(GL_TEXTURE_2D, g_fbo_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_viewport_width, g_viewport_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_fbo_texture, 0);

        // Resize depth buffer
        glBindRenderbuffer(GL_RENDERBUFFER, g_depth_buffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, g_viewport_width, g_viewport_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_depth_buffer);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

		gse::window::set_fbo(g_fbo, { g_viewport_width, g_viewport_height });
    }

    const ImVec2 image_position = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(g_fbo_texture)), ImVec2(static_cast<float>(g_viewport_width), static_cast<float>(g_viewport_height)), ImVec2(0, 1), ImVec2(1, 0));

    g_game_window_position = image_position;
    g_game_window_size = ImVec2(static_cast<float>(g_viewport_width), static_cast<float>(g_viewport_height));

    ImGui::End();

    ImGui::Begin("Editor");

    const auto scenes = gse::g_scene_handler.get_all_scenes();
    const auto active_scenes = gse::g_scene_handler.get_active_scenes();
    const std::string current_scene_name = active_scenes.empty() ? "No Active Scene" : active_scenes[0]->get_id().lock()->tag;
    if (scenes.empty()) {
        ImGui::Text("No scenes available.");
    }
    else if (scenes.size() == 1) {
        ImGui::Text("Only one scene available: %s", scenes[0]->tag.c_str());
    }
    else {
        if (ImGui::BeginCombo("Select Scene", current_scene_name.c_str())) {
            for (const auto& scene : scenes) {
                const bool is_selected = !active_scenes.empty() && active_scenes[0]->get_id().lock()->tag == scene->tag;
                if (ImGui::Selectable(scene->tag.c_str(), is_selected)) {
                    if (!is_selected) { // Avoid redundant activation
                        gse::g_scene_handler.activate_scene(scene);
                    }
                }
                if (is_selected) {
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


    gse::debug::render_imgui();
}

void editor::exit() {
    glDeleteFramebuffers(1, &g_fbo);
    glDeleteTextures(1, &g_fbo_texture);
    glDeleteRenderbuffers(1, &g_depth_buffer);
}

