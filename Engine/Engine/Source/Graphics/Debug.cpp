#include "Graphics/Debug.h"
#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Core/Clock.h"
#include "Core/EngineCore.h"
#include "Core/JsonParser.h"
#include "Platform/GLFW/Window.h"

struct WindowState {
	ImVec2 position;
	ImVec2 size;
};

void to_json(nlohmann::json& j, const ImVec2& v) {
	j = nlohmann::json{ {"x", v.x}, {"y", v.y} };
}

void from_json(const nlohmann::json& j, ImVec2& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WindowState, position, size);

std::string imguiSaveFilePath;

void Engine::Debug::setImguiSaveFilePath(const std::string& path) {
	imguiSaveFilePath = path;
}

std::unordered_map<std::string, WindowState> windowStates;
Engine::Clock autosaveClock;
const Engine::Units::Seconds autosaveTime = 60.f;

void Engine::Debug::setUpImGui() {
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

	JsonParse::parse(JsonParse::loadJson(imguiSaveFilePath), [](const std::string& key, const nlohmann::json& value) {
		if (value.is_null() || !value.is_object() || !value.contains("position") || !value.contains("size")) {
			return;
		}
		windowStates[key] = value.get<WindowState>();
	});
}

void Engine::Debug::updateImGui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Save State")) {
				saveImGuiState();
			}
			if (ImGui::MenuItem("Exit")) {
				requestShutdown();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// Autosave
	if (autosaveClock.getElapsedTime() > autosaveTime) {
		saveImGuiState();
		autosaveClock.reset();
	}
}

void Engine::Debug::renderImGui() {
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

void Engine::Debug::saveImGuiState() {
	nlohmann::json json;
	for (const auto& [fst, snd] : windowStates) {
		const std::string& name = fst;
		const WindowState& state = snd;
		json[name] = state;
	}
	JsonParse::writeJson(imguiSaveFilePath, [&json](nlohmann::json& j) {
		j = json;
	});
}

void Engine::Debug::createWindow(const std::string& name, const ImVec2& size, const ImVec2& position, bool open) {
	if (windowStates.contains(name)) {
		const auto& [position, size] = windowStates[name];
		ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
	}
	else {
		ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
		windowStates[name] = { position, size };
	}

	ImGui::Begin(name.c_str(), &open);

	windowStates[name].position = ImGui::GetWindowPos();
	windowStates[name].size = ImGui::GetWindowSize();
}

void Engine::Debug::printVector(const std::string& name, const glm::vec3& vec, const char* unit) {
	if (unit) {
		ImGui::InputFloat3((name + " - " + unit).c_str(), const_cast<float*>(&vec.x));
	}
	else {
		ImGui::InputFloat3(name.c_str(), const_cast<float*>(&vec.x));
	}
}

void Engine::Debug::printValue(const std::string& name, const float& value, const char* unit) {
	if (unit) {
		ImGui::InputFloat((name + " - " + unit).c_str(), const_cast<float*>(&value), 0.f, 0.f, "%.10f");
	}
	else {
		ImGui::InputFloat(name.c_str(), const_cast<float*>(&value));
	}
}

void Engine::Debug::printBoolean(const std::string& name, const bool& value) {
	ImGui::Checkbox(name.c_str(), const_cast<bool*>(&value));
}
