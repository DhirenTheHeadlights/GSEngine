module;

#include "imgui.h"
#include "imguiThemes.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

export module gse.graphics.debug;

import std;
import vulkan_hpp;

import gse.physics.math;
import gse.core.clock;
import gse.core.json_parser;
import gse.platform;

export namespace gse::debug {
	auto initialize_imgui(const vulkan::config& config) -> void;
    auto update_imgui() -> void;
	auto render_imgui(const vk::CommandBuffer& command_buffer) -> void;
    auto save_imgui_state() -> void;

    auto set_imgui_save_file_path(const std::filesystem::path& path) -> void;

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
constexpr gse::time g_autosave_time = gse::seconds(60.f);
std::filesystem::path g_imgui_save_file_path;
std::vector<std::function<void()>> g_render_call_backs;

auto gse::debug::set_imgui_save_file_path(const std::filesystem::path& path) -> void {
    g_imgui_save_file_path = path;
}

auto gse::debug::initialize_imgui(const vulkan::config& config) -> void {
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    imguiThemes::embraceTheDarkness();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoTaskBarIcon = true;

    ImGui_ImplGlfw_InitForVulkan(window::get_window(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = config.instance_data.instance;
    init_info.PhysicalDevice = config.device_data.physical_device;
	init_info.Device = config.device_data.device;
	init_info.QueueFamily = vulkan::find_queue_families(config.device_data.physical_device, config.instance_data.surface).graphics_family.value();
	init_info.Queue = config.queue.graphics;
    init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = config.descriptor.pool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info, config.render_pass);

    const vk::CommandBuffer cmd = vulkan::begin_single_line_commands(config);

    ImGui_ImplVulkan_CreateFontsTexture(cmd);

    end_single_line_commands(cmd, config);

    ImGui_ImplVulkan_DestroyFontUploadObjects();

    if (exists(g_imgui_save_file_path)) {
        ImGui::LoadIniSettingsFromDisk(g_imgui_save_file_path.string().c_str());
    }
}

auto gse::debug::update_imgui() -> void {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (const ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
    }
}

auto gse::debug::render_imgui(const vk::CommandBuffer& command_buffer) -> void {
    for (const auto& callback : g_render_call_backs) {
        callback();
    }
    g_render_call_backs.clear();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
	ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

auto gse::debug::save_imgui_state() -> void {
    ImGui::SaveIniSettingsToDisk(g_imgui_save_file_path.string().c_str());
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
