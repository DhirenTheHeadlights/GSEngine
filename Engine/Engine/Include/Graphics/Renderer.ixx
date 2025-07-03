export module gse.graphics:renderer;

import std;

import :debug;
import :renderer3d;
import :renderer2d;
import :shader_loader;
import :texture_loader;
import :model_loader;
import :gui;
import :render_component;

import gse.utility;
import gse.platform;

export namespace gse::renderer {
	auto initialize() -> void;
	auto update() -> void;
	auto render(const std::function<void()>& in_frame, std::span<render_component> components) -> void;
	auto shutdown() -> void;
}

namespace gse::renderer {
	auto begin_frame() -> void;
	auto end_frame() -> void;

	std::optional<vulkan::config> g_config;
	renderer3d::context g_renderer3d_context;
	renderer2d::context g_renderer2d_context;
	std::optional<font> g_gui_font;
	shader_loader::shader_context g_shader_context;
	model_loader::model_context g_model_loader_context;
	std::optional<texture_loader::texture_loader_context> g_texture_loader_context;
}

auto gse::renderer::initialize() -> void {
	g_config.emplace(platform::initialize());
	g_texture_loader_context.emplace();
	g_gui_font.emplace();

	model_loader::initialize(g_model_loader_context);
	load_shaders(g_shader_context, g_config->device_data.device);
	renderer3d::initialize(g_renderer3d_context, *g_config);
	renderer2d::initialize(g_renderer2d_context, *g_config);
	gui::initialize(&*g_gui_font, *g_config);
	texture_loader::initialize(*g_texture_loader_context, *g_config);
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

auto gse::renderer::render(const std::function<void()>& in_frame, const std::span<render_component> components) -> void {
	gui::render();

	begin_frame();
	render_geometry(g_renderer3d_context, *g_config, components);
	render_lighting(g_renderer3d_context, *g_config, components);
	renderer2d::render(g_renderer2d_context, *g_config);
	in_frame();
	end_frame();
}

auto gse::renderer::end_frame() -> void {
	vulkan::end_frame(window::get_window(), *g_config);
	window::end_frame();
	vulkan::transient_allocator::end_frame();
}

auto gse::renderer::shutdown() -> void {
	g_config->device_data.device.waitIdle();
	gui::shutdown();
	g_texture_loader_context.reset();
	g_gui_font.reset();
	g_model_loader_context = {};
	g_shader_context = {};
	g_renderer2d_context = {};
	g_renderer3d_context = {};
	g_config.reset();
	platform::shutdown();
}