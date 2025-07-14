export module gse.graphics:renderer;

import std;

import :debug;
import :lighting_renderer;
import :geometry_renderer;
import :renderer2d;
import :gui;
import :render_component;
import :resource_loader;
import :material;
import :rendering_context;

import gse.utility;
import gse.platform;

namespace gse {
	renderer::context g_rendering_context;
}

export namespace gse {
	template <typename Resource>
	auto get(const id& id) -> typename Resource::handle {
		return g_rendering_context.resource<Resource>(id);
	}

	template <typename Resource>
	auto queue(const std::filesystem::path& path, const std::string& name) -> void {
		g_rendering_context.queue<Resource>(path, name);
	}

	template <typename Resource, typename... Args>
	auto queue(const id& id, Args&&... args) -> void {
		g_rendering_context.queue<Resource>(id, std::forward<Args>(args)...);
	}

	template <typename Resource>
	auto instantly_load(const id& id) -> void {
		g_rendering_context.instantly_load<Resource>(id);
	}

	template <typename Resource>
	auto add(Resource&& resource) -> void {
		g_rendering_context.add<Resource>(std::forward<Resource>(resource));
	}

	template <typename Resource>
	auto resource_state(const id& id) -> resource_loader_base::state {
		return g_rendering_context.resource_state<Resource>(id);
	}
}

export namespace gse::renderer {
	auto initialize() -> void;
	auto update() -> void;
	auto render(const std::function<void()>& in_frame) -> void;
	auto shutdown() -> void;
}

namespace gse::renderer {
	auto begin_frame() -> void;
	auto end_frame() -> void;

	renderer3d::context g_renderer3d_context;
	renderer2d::context g_renderer2d_context;
}

auto gse::renderer::initialize() -> void {
	g_rendering_context.add_loader<texture>();
	g_rendering_context.add_loader<model>();
	g_rendering_context.add_loader<shader>();
	g_rendering_context.add_loader<font>();

	renderer3d::initialize(g_renderer3d_context, g_rendering_context.config());
	renderer2d::initialize(g_renderer2d_context, g_rendering_context.config());
	gui::initialize(g_rendering_context);
}

auto gse::renderer::begin_frame() -> void {
	g_rendering_context.flush_queues();

	window::begin_frame();
	vulkan::begin_frame(window::get_window(), g_rendering_context.config());
}

auto gse::renderer::update() -> void {
	window::update();
	gui::update();
}

auto gse::renderer::render(const std::function<void()>& in_frame) -> void {
	gui::render();
	begin_frame();
	render_geometry(g_renderer3d_context, g_rendering_context.config());
	render_lighting(g_renderer3d_context, g_rendering_context.config());
	renderer2d::render(g_renderer2d_context, g_rendering_context.config());
	in_frame();
	end_frame();
}

auto gse::renderer::end_frame() -> void {
	vulkan::end_frame(window::get_window(), g_rendering_context.config());
	window::end_frame();
	vulkan::transient_allocator::end_frame();
}

auto gse::renderer::shutdown() -> void {
	g_rendering_context.config().device_data.device.waitIdle();
	debug::shutdown_imgui();
	gui::shutdown();
	g_renderer2d_context = {};
	g_renderer3d_context = {};
	g_rendering_context.config().swap_chain_data.albedo_image = {};
	g_rendering_context.config().swap_chain_data.normal_image = {};
	g_rendering_context.config().swap_chain_data.depth_image = {};
	platform::shutdown();
}