export module  gse.graphics.renderer;

import std;
import vulkan_hpp;

import gse.graphics.debug;
import gse.graphics.renderer3d;
import gse.graphics.renderer2d;
import gse.graphics.shader_loader;
import gse.graphics.gui;
import gse.platform;

export namespace gse::renderer {
	auto initialize() -> void;
	auto begin_frame() -> void;
	auto update() -> void;
	auto render() -> void;
	auto end_frame() -> void;
	auto shutdown() -> void;
}

gse::vulkan::config g_config;

auto gse::renderer::initialize() -> void {
	g_config = platform::initialize();
	shader_loader::load_shaders(TODO);
	renderer3d::initialize(g_config);
	renderer2d::initialize(g_config);
	gui::initialize();
}

auto gse::renderer::begin_frame() -> void {
	window::begin_frame();
	vulkan::begin_frame(window::get_window(), g_config);
	renderer3d::begin_frame(g_config);
	renderer2d::begin_frame(g_config);
}

auto gse::renderer::update() -> void {
	window::update(); 
	gui::update();
}

auto gse::renderer::render() -> void {
	renderer3d::render(g_config);
	renderer2d::render(g_config);
	gui::render();
	std::cout << "Rendering frame..." << std::endl;
}

auto gse::renderer::end_frame() -> void {
	renderer3d::end_frame(g_config);
	renderer2d::end_frame(g_config);
	vulkan::end_frame(g_config);
	window::end_frame();
	vulkan::transient_allocator::end_frame();
}

auto gse::renderer::shutdown() -> void {
	renderer2d::shutdown(g_config);
	renderer3d::shutdown(g_config);
	window::shutdown();
	platform::shutdown(g_config);
}