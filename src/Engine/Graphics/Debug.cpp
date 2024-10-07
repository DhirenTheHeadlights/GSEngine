#include "Engine/Graphics/Debug.h"
#include <imguiThemes.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Engine/Platform/Platform.h"

#include "Engine/Physics/Vector/Vec3.h"

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

	ImGui_ImplGlfw_InitForOpenGL(Engine::Platform::window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void Engine::Debug::updateImGui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
}

void Engine::Debug::renderImGui() {
	ImGui::Render();

	int displayW, displayH;
	glfwGetFramebufferSize(Engine::Platform::window, &displayW, &displayH);
	glViewport(0, 0, displayW, displayH);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	const ImGuiIO& io = ImGui::GetIO(); (void)io;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backupCurrentContext);
	}
}
