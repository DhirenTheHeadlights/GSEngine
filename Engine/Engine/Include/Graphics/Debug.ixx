module;

#include "imgui.h"
#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "glad/glad.h"
#include "glfw/glfw3.h"

export module gse.graphics.debug;

import std;

import gse.physics.math;
import gse.core.clock;
import gse.core.json_parser;
import gse.platform.glfw.window;

export namespace gse::debug {
	auto set_up_imgui() -> void;
	auto update_imgui() -> void;
	auto render_imgui() -> void;
	auto save_imgui_state() -> void;

	auto set_imgui_save_file_path(const std::string& path) -> void;

	auto print_vector(const std::string& name, const unitless::vec3& vec, const char* unit = nullptr) -> void;
	auto print_value(const std::string& name, const float& value, const char* unit = nullptr) -> void;

	template <internal::is_unit UnitType, internal::is_quantity QuantityType>
	auto unit_slider(const std::string& name, QuantityType& quantity, const QuantityType& min, const QuantityType& max) -> void {
		float value = quantity.template as<UnitType>();

		const std::string slider_label = name + " (" + UnitType::unit_name + ")";
		if (ImGui::SliderFloat(slider_label.c_str(),
			&value,
			min.template as<UnitType>(),
			max.template as<UnitType>()))
		{
			quantity.template set<UnitType>(value);
		}
	}

	auto unit_slider(const std::string& name, float& val, const float& min, const float& max) -> void {
		const std::string slider_label = name + " (unitless)";
		ImGui::SliderFloat(slider_label.c_str(), &val, min, max);
	}

	auto print_boolean(const std::string& name, const bool& value) -> void;

	auto add_imgui_callback(const std::function<void()>& callback) -> void;
	auto get_imgui_needs_inputs() -> bool;
}

gse::clock g_autosave_clock;
const gse::time g_autosave_time = gse::seconds(60.f);
std::string g_imgui_save_file_path;

std::vector<std::function<void()>> g_render_call_backs;

auto gse::debug::set_imgui_save_file_path(const std::string& path) -> void {
	g_imgui_save_file_path = path;
}

auto gse::debug::set_up_imgui() -> void {
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

auto gse::debug::update_imgui() -> void {
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
				//request_shutdown();
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

auto gse::debug::render_imgui() -> void {
	for (const auto& callback : g_render_call_backs) {
		callback();
	}
	g_render_call_backs.clear();

	ImGui::Render();

	const unitless::vec2i window_size = window::get_window_size();
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

auto gse::debug::save_imgui_state() -> void {
	ImGui::SaveIniSettingsToDisk(g_imgui_save_file_path.c_str());
}

auto gse::debug::print_vector(const std::string& name, const unitless::vec3& vec, const char* unit) -> void {
	if (unit) {
		ImGui::InputFloat3((name + " - " + unit).c_str(), const_cast<float*>(&vec.x));
	}
	else {
		ImGui::InputFloat3(name.c_str(), const_cast<float*>(&vec.x));
	}
}

auto gse::debug::print_value(const std::string& name, const float& value, const char* unit) -> void {
	if (unit) {
		ImGui::InputFloat((name + " - " + unit).c_str(), const_cast<float*>(&value), 0.f, 0.f, "%.10f");
	}
	else {
		ImGui::InputFloat(name.c_str(), const_cast<float*>(&value));
	}
}

auto gse::debug::print_boolean(const std::string& name, const bool& value) -> void {
	ImGui::Checkbox(name.c_str(), const_cast<bool*>(&value));
}

auto gse::debug::add_imgui_callback(const std::function<void()>& callback) -> void {
	g_render_call_backs.push_back(callback);
}

auto gse::debug::get_imgui_needs_inputs() -> bool {
	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard || io.WantTextInput || io.WantCaptureMouse;
}