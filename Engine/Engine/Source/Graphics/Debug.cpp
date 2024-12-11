#include "Graphics/Debug.h"
#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Core/Clock.h"
#include "Core/EngineCore.h"
#include "Core/JsonParser.h"
#include "Platform/GLFW/Window.h"

namespace {
	gse::clock autosaveClock;
	const gse::time autosaveTime = gse::seconds(60.f);
	std::string imguiSaveFilePath;

	std::vector<std::function<void()>> renderCallBacks;
}

void gse::debug::set_imgui_save_file_path(const std::string& path) {
	imguiSaveFilePath = path;
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

	if (std::filesystem::exists(imguiSaveFilePath)) {
		ImGui::LoadIniSettingsFromDisk(imguiSaveFilePath.c_str());
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

	if (autosaveClock.get_elapsed_time() > autosaveTime) {
		save_imgui_state();
		autosaveClock.reset();
	}
}

void gse::debug::render_imgui() {
	for (const auto& callback : renderCallBacks) {
		callback();
	}
	renderCallBacks.clear();

	ImGui::Render();

	const glm::ivec2 windowSize = window::get_window_size();
	glViewport(0, 0, windowSize.x, windowSize.y);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	const ImGuiIO& io = ImGui::GetIO(); (void)io;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backupCurrentContext);
	}
}

void gse::debug::save_imgui_state() {
	ImGui::SaveIniSettingsToDisk(imguiSaveFilePath.c_str());
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
	renderCallBacks.push_back(callback);
}

bool gse::debug::get_imgui_needs_inputs() {
	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard || io.WantTextInput || io.WantCaptureMouse;
}