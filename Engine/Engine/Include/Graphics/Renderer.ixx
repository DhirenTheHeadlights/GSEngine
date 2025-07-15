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
	static constinit renderer::context rendering_context;
	static constinit std::unordered_map<std::type_index, std::unique_ptr<base_renderer>> renderers;
}

export namespace gse {
	template <typename Resource>
	auto get(const id& id) -> typename Resource::handle {
		return rendering_context.resource<Resource>(id);
	}

	template <typename Resource>
	auto queue(const std::filesystem::path& path, const std::string& name) -> void {
		rendering_context.queue<Resource>(path, name);
	}

	template <typename Resource, typename... Args>
	auto queue(const id& id, Args&&... args) -> void {
		rendering_context.queue<Resource>(id, std::forward<Args>(args)...);
	}

	template <typename Resource>
	auto instantly_load(const id& id) -> void {
		rendering_context.instantly_load<Resource>(id);
	}

	template <typename Resource>
	auto add(Resource&& resource) -> void {
		rendering_context.add<Resource>(std::forward<Resource>(resource));
	}

	template <typename Resource>
	auto resource_state(const id& id) -> resource_loader_base::state {
		return rendering_context.resource_state<Resource>(id);
	}
}

export namespace gse::renderer {
	auto initialize(std::span<std::reference_wrapper<registry>> registries) -> void;
	auto update() -> void;
	auto render(const std::function<void()>& in_frame = {}) -> void;
	auto shutdown() -> void;
}

namespace gse::renderer {
	auto begin_frame() -> void;
	auto end_frame() -> void;

	template <typename T>
	auto renderer() -> T& {
		if (const auto it = renderers.find(std::type_index(typeid(T))); it != renderers.end()) {
			return static_cast<T&>(*it->second);
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

	renderers.emplace(std::type_index(typeid(geometry)), std::unique_ptr<base_renderer>(std::make_unique<geometry>(rendering_context, registries)));
	renderers.emplace(std::type_index(typeid(lighting)), std::unique_ptr<base_renderer>(std::make_unique<lighting>(rendering_context, registries)));
	renderers.emplace(std::type_index(typeid(sprite)), std::unique_ptr<base_renderer>(std::make_unique<sprite>(rendering_context, registries)));
	renderers.emplace(std::type_index(typeid(text)), std::unique_ptr<base_renderer>(std::make_unique<text>(rendering_context, registries)));

	for (const auto& renderer : renderers | std::views::values) {
		renderer->initialize();
	}

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

	for (const auto& renderer : renderers | std::views::values) {
		renderer->render();
	}

	in_frame();
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

	for (auto& renderer : renderers | std::views::values) {
		renderer.reset();
	}

	rendering_context.config().swap_chain_data.albedo_image = {};
	rendering_context.config().swap_chain_data.normal_image = {};
	rendering_context.config().swap_chain_data.depth_image = {};

	platform::shutdown();
}