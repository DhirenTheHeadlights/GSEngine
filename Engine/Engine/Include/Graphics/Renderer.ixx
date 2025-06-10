export module  gse.graphics.renderer;

import std;
import vulkan_hpp;

import gse.core.timer;
import gse.graphics.debug;
import gse.graphics.renderer3d;
import gse.graphics.renderer2d;
import gse.graphics.shader_loader;
import gse.graphics.texture_loader;
import gse.graphics.model_loader;
import gse.graphics.gui;
import gse.platform;

export namespace gse::renderer {
	auto initialize() -> void;
	auto update() -> void;
	auto render(const std::function<void()>& in_frame) -> void;
	auto shutdown() -> void;
}

namespace gse::renderer {
	auto begin_frame() -> void;
	auto end_frame() -> void;
}

std::optional<gse::vulkan::config> g_config;

auto gse::renderer::initialize() -> void {
	g_config.emplace(platform::initialize());

	shader_loader::load_shaders(g_config->device_data.device);
	renderer3d::initialize(*g_config);
	renderer2d::initialize(*g_config);
	gui::initialize();
}

auto gse::renderer::begin_frame() -> void {
	texture_loader::load_queued_textures(*g_config);
	model_loader::load_queued_models(*g_config);
	window::begin_frame();
	vulkan::begin_frame(window::get_window(), *g_config);
}

auto gse::renderer::update() -> void {
	window::update();
	gui::update();
}

auto gse::renderer::render(const std::function<void()>& in_frame) -> void {
	begin_frame();
	renderer3d::render(*g_config);
	g_config->frame_context.command_buffer.nextSubpass(vk::SubpassContents::eInline);
	renderer2d::render(*g_config);
	gui::render();
	in_frame();
	end_frame();
}

auto gse::renderer::end_frame() -> void {
	vulkan::end_frame(window::get_window(), *g_config);
	window::end_frame();
	vulkan::transient_allocator::end_frame();
}

auto gse::renderer::shutdown() -> void {
	renderer2d::shutdown(g_config->device_data);
	renderer3d::shutdown(*g_config);
	platform::shutdown(*g_config);
	g_config.reset();
}