export module  gse.graphics.renderer;

import std;
import vulkan_hpp;

import gse.graphics.renderer3d;
import gse.graphics.renderer2d;
import gse.graphics.shader_loader;
import gse.platform.glfw.window;

export namespace gse::renderer {
	auto initialize() -> void;
	auto begin_frame() -> void;
	auto render() -> void;
	auto end_frame() -> void;
	auto shutdown() -> void;
}

auto gse::renderer::initialize() -> void {
	window::initialize();
	shader_loader::load_shaders();
	renderer3d::initialize();
	renderer2d::initialize();
}

auto gse::renderer::begin_frame() -> void {
	window::begin_frame();
	renderer3d::begin_frame();
	renderer2d::begin_frame();
}

auto gse::renderer::render() -> void {
	renderer3d::render();
	renderer2d::render();
}

auto gse::renderer::end_frame() -> void {
	renderer3d::end_frame();
	renderer2d::end_frame();
	window::end_frame();
}

auto gse::renderer::shutdown() -> void {
	renderer2d::shutdown();
	renderer3d::shutdown();
	window::shutdown();
}