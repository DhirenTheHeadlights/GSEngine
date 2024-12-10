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
	const gse::Time autosaveTime = gse::seconds(60.f);
	std::string imguiSaveFilePath;

	std::vector<std::function<void()>> renderCallBacks;
}

void gse::Debug::setImguiSaveFilePath(const std::string& path) {
	imguiSaveFilePath = path;
}

void gse::Debug::setUpImGui() {
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

	ImGui_ImplGlfw_InitForOpenGL(Window::getWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 330");

	if (std::filesystem::exists(imguiSaveFilePath)) {
		ImGui::LoadIniSettingsFromDisk(imguiSaveFilePath.c_str());
	}
}

void gse::Debug::updateImGui() {
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
				saveImGuiState();
			}
			if (ImGui::MenuItem("Exit")) {
				request_shutdown();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (io.WantCaptureMouse) {
		Window::setMouseVisible(true);
	}

	if (autosaveClock.get_elapsed_time() > autosaveTime) {
		saveImGuiState();
		autosaveClock.reset();
	}
}

void gse::Debug::renderImGui() {
	for (const auto& callback : renderCallBacks) {
		callback();
	}
	renderCallBacks.clear();

	ImGui::Render();

	const glm::ivec2 windowSize = Window::getWindowSize();
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

void gse::Debug::saveImGuiState() {
	ImGui::SaveIniSettingsToDisk(imguiSaveFilePath.c_str());
}

void gse::Debug::printVector(const std::string& name, const glm::vec3& vec, const char* unit) {
	if (unit) {
		ImGui::InputFloat3((name + " - " + unit).c_str(), const_cast<float*>(&vec.x));
	}
	else {
		ImGui::InputFloat3(name.c_str(), const_cast<float*>(&vec.x));
	}
}

void gse::Debug::printValue(const std::string& name, const float& value, const char* unit) {
	if (unit) {
		ImGui::InputFloat((name + " - " + unit).c_str(), const_cast<float*>(&value), 0.f, 0.f, "%.10f");
	}
	else {
		ImGui::InputFloat(name.c_str(), const_cast<float*>(&value));
	}
}

void gse::Debug::printBoolean(const std::string& name, const bool& value) {
	ImGui::Checkbox(name.c_str(), const_cast<bool*>(&value));
}

void gse::Debug::addImguiCallback(const std::function<void()>& callback) {
	renderCallBacks.push_back(callback);
}

bool gse::Debug::getImGuiNeedsInputs() {
	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard || io.WantTextInput || io.WantCaptureMouse;
}