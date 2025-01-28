module;

#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "glad/glad.h"
#include "glfw/glfw3.h"

module gse.graphics.debug;

import std;

import gse.core.clock;
import gse.core.engine;
import gse.core.json_parser;
import gse.platform.glfw.window;

namespace {
	gse::clock g_autosave_clock;
	const gse::time g_autosave_time = gse::seconds(60.f);
	std::string g_imgui_save_file_path;

	std::vector<std::function<void()>> g_render_call_backs;
}

void gse::debug::set_imgui_save_file_path(const std::string& path) {
	g_imgui_save_file_path = path;
}

void gse::debug::set_up_imgui() {
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	imguiThemes::embraceTheDarkness();

	ImGuiIO& io = ImGui::GetIO(); (void)io;						// void cast prevents unused variable warning
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	io.ConfigViewportsNoTaskBarIcon = true;

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.Colors[ImGuiCol_WindowBg].w = 0.f;
		style.Colors[ImGuiCol_DockingEmptyBg].w = 0.f;
	}

	ImGui_ImplGlfw_InitForOpenGL(window::get_window(), true);
	ImGui_ImplOpenGL3_Init("#version 330");

	if (std::filesystem::exists(g_imgui_save_file_path)) {
		ImGui::LoadIniSettingsFromDisk(g_imgui_save_file_path.c_str());
	}
}

void gse::debug::update_imgui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	const ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
	}

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Save State")) {
				save_imgui_state();
			}
			if (ImGui::MenuItem("Exit")) {
				request_shutdown();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (io.WantCaptureMouse) {
		window::set_mouse_visible(true);
	}

	if (g_autosave_clock.get_elapsed_time() > g_autosave_time) {
		save_imgui_state();
		g_autosave_clock.reset();
	}
}

void gse::debug::render_imgui() {
	for (const auto& callback : g_render_call_backs) {
		callback();
	}
	g_render_call_backs.clear();

	ImGui::Render();

	const glm::ivec2 window_size = window::get_window_size();
	glViewport(0, 0, window_size.x, window_size.y);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	const ImGuiIO& io = ImGui::GetIO(); (void)io;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
}

void gse::debug::save_imgui_state() {
	ImGui::SaveIniSettingsToDisk(g_imgui_save_file_path.c_str());
}

void gse::debug::print_vector(const std::string& name, const glm::vec3& vec, const char* unit) {
	if (unit) {
		ImGui::InputFloat3((name + " - " + unit).c_str(), const_cast<float*>(&vec.x));
	}
	else {
		ImGui::InputFloat3(name.c_str(), const_cast<float*>(&vec.x));
	}
}

void gse::debug::print_value(const std::string& name, const float& value, const char* unit) {
	if (unit) {
		ImGui::InputFloat((name + " - " + unit).c_str(), const_cast<float*>(&value), 0.f, 0.f, "%.10f");
	}
	else {
		ImGui::InputFloat(name.c_str(), const_cast<float*>(&value));
	}
}

void gse::debug::print_boolean(const std::string& name, const bool& value) {
	ImGui::Checkbox(name.c_str(), const_cast<bool*>(&value));
}

void gse::debug::add_imgui_callback(const std::function<void()>& callback) {
	g_render_call_backs.push_back(callback);
}

bool gse::debug::get_imgui_needs_inputs() {
	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard || io.WantTextInput || io.WantCaptureMouse;
}