export module gse.graphics:renderer;

import std;

import :debug;
import :lighting_renderer;
import :geometry_renderer;
import :sprite_renderer;
import :text_renderer;
import :gui;
import :render_component;
import :resource_loader;
import :material;
import :rendering_context;

import gse.utility;
import gse.platform;

namespace gse {
	renderer::context rendering_context;
	std::vector<std::unique_ptr<base_renderer>> renderers;
}

export namespace gse {
	template <typename Resource>
	auto get(const id& id) -> resource::handle<Resource> {
		return rendering_context.get<Resource>(id);
	}

	template <typename Resource>
	auto get(const std::string& filename) -> resource::handle<Resource> {
		return rendering_context.get<Resource>(filename);
	}

	template <typename Resource, typename... Args>
	auto queue(const std::string& name, Args&&... args) -> resource::handle<Resource> {
		return rendering_context.queue<Resource>(name, std::forward<Args>(args)...);
	}

	template <typename Resource>
	auto instantly_load(const id& id) -> resource::handle<Resource> {
		return rendering_context.instantly_load<Resource>(id);
	}

	template <typename Resource>
	auto add(Resource&& resource) -> void {
		rendering_context.add<Resource>(std::forward<Resource>(resource));
	}

	template <typename Resource>
	auto resource_state(const id& id) -> resource::state {
		return rendering_context.resource_state<Resource>(id);
	}
}

export namespace gse::renderer {
	auto initialize(std::span<std::reference_wrapper<registry>> registries) -> void;
	auto update() -> void;
	auto render(const std::function<void()>& in_frame = {}) -> void;
	auto shutdown() -> void;
	auto camera() -> camera&;
}

namespace gse::renderer {
	auto begin_frame() -> void;
	auto end_frame() -> void;

	template <typename T>
	auto renderer() -> T& {
		for (const auto& renderer_ptr : renderers) {
			if (auto* p = dynamic_cast<T*>(renderer_ptr.get())) {
				return *p;
			}
		}
		throw std::runtime_error("Renderer not found: " + std::string(typeid(T).name()));
	}
}

auto gse::renderer::initialize(const std::span<std::reference_wrapper<registry>> registries) -> void {
	rendering_context.add_loader<texture>();
	rendering_context.add_loader<model>();
	rendering_context.add_loader<shader>();
	rendering_context.add_loader<font>();
	rendering_context.add_loader<material>();

	rendering_context.compile();

	renderers.push_back(std::make_unique<geometry>(rendering_context, registries));
	renderers.push_back(std::make_unique<lighting>(rendering_context, registries));
	renderers.push_back(std::make_unique<sprite>(rendering_context, registries));
	renderers.push_back(std::make_unique<text>(rendering_context, registries));

	for (const auto& renderer : renderers) {
		renderer->initialize();
	}

	debug::initialize_imgui(rendering_context.config());
	gui::initialize(rendering_context);
}

auto gse::renderer::begin_frame() -> void {
	rendering_context.flush_queues();
	window::begin_frame();
	vulkan::begin_frame(window::get_window(), rendering_context.config());
}

auto gse::renderer::update() -> void {
	window::update();
	gui::update();
}

auto gse::renderer::render(const std::function<void()>& in_frame) -> void {
	gui::render(rendering_context, renderer<sprite>(), renderer<text>());
	begin_frame();

	{
		std::lock_guard lock(*rendering_context.config().command.pool_mutex);
		std::lock_guard queue(*rendering_context.config().queue.mutex);
		for (const auto& renderer : renderers) {
			renderer->render();
		}
		in_frame();
	}

	end_frame();
}

auto gse::renderer::end_frame() -> void {
	vulkan::end_frame(window::get_window(), rendering_context.config());
	window::end_frame();
	vulkan::transient_allocator::end_frame();
}

auto gse::renderer::shutdown() -> void {
	rendering_context.config().device_data.device.waitIdle();
	debug::shutdown_imgui();

	for (auto& renderer : renderers) {
		renderer.reset();
	}

	rendering_context.config().swap_chain_data.albedo_image = {};
	rendering_context.config().swap_chain_data.normal_image = {};
	rendering_context.config().swap_chain_data.depth_image = {};

	platform::shutdown();
}

auto gse::renderer::camera() -> gse::camera& {
	return rendering_context.camera();
}