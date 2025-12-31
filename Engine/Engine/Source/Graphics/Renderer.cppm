export module gse.graphics:renderer;

import std;

import :render_component;
import :resource_loader;
import :material;
import :rendering_context;
import :font;
import :model;
import :shader;
import :texture;

import gse.utility;
import gse.platform;

export namespace gse::renderer {
	class system final : public basic_system {
	public:
		explicit system(
			context& context
		);

		auto initialize(
		) const -> void;

		auto update(
		) const -> void;

		auto begin_frame(
		) -> bool override;

		auto end_frame(
		) -> void override;

		auto shutdown(
		) -> void override;

		auto camera(
		) const -> camera&;

		auto set_ui_focus(
			bool focus
		) const -> void;

		auto frame_begun(
		) const -> bool;

		template <typename Resource>
		auto get(
			const id& id
		) -> resource::handle<Resource>;

		template <typename Resource>
		auto get(
			const std::string& filename
		) -> resource::handle<Resource>;

		template <typename Resource, typename... Args>
		auto queue(
			const std::string& name,
			Args&&... args
		) -> resource::handle<Resource>;

		template <typename Resource>
		auto instantly_load(
			const id& id
		) -> resource::handle<Resource>;

		template <typename Resource>
		auto add(
			Resource&& resource
		) -> void;

		template <typename Resource>
		auto resource_state(
			const id& id
		) const -> resource::state;
	private:
		context* m_context = nullptr;
		bool m_frame_begun = false;
	};
}

gse::renderer::system::system(context& context) : m_context(std::addressof(context)) {
}

auto gse::renderer::system::initialize() const -> void {
	auto& ctx = *m_context;

	ctx.add_loader<texture>();
	ctx.add_loader<model>();
	ctx.add_loader<shader>();
	ctx.add_loader<font>();
	ctx.add_loader<material>();

	ctx.compile();
}

auto gse::renderer::system::update() const -> void {
	auto& ctx = *m_context;

	ctx.process_resource_queue();
	ctx.window().update(ctx.ui_focus());
	ctx.camera().update_orientation();

	if (!ctx.ui_focus()) {
		ctx.camera().process_mouse_movement(system_of<input::system>().current_state().mouse_delta());
	}
}

auto gse::renderer::system::begin_frame() -> bool {
	auto& ctx = *m_context;

	ctx.process_gpu_queue();

	m_frame_begun = vulkan::begin_frame({
		.window = ctx.window().raw_handle(),
		.frame_buffer_resized = ctx.window().frame_buffer_resized(),
		.minimized = ctx.window().minimized(),
		.config = ctx.config()
	});

	return m_frame_begun;
}

auto gse::renderer::system::end_frame() -> void {
	if (!m_frame_begun) {
		return;
	}

	auto& ctx = *m_context;

	vulkan::end_frame({
		.window = ctx.window().raw_handle(),
		.frame_buffer_resized = ctx.window().frame_buffer_resized(),
		.minimized = ctx.window().minimized(),
		.config = ctx.config()
	});

	vulkan::transient_allocator::end_frame();

	m_frame_begun = false;
}

auto gse::renderer::system::shutdown() -> void {
	auto& ctx = *m_context;

	ctx.config().device_config().device.waitIdle();
	ctx.shutdown();

	vulkan::persistent_allocator::clean_up();
	shader::destroy_global_layouts();
}

auto gse::renderer::system::camera() const -> gse::camera& {
	return m_context->camera();
}

auto gse::renderer::system::set_ui_focus(const bool focus) const -> void {
	m_context->set_ui_focus(focus);
}

auto gse::renderer::system::frame_begun() const -> bool {
	return m_frame_begun;
}

template <typename Resource>
auto gse::renderer::system::get(const id& id) -> resource::handle<Resource> {
	return m_context->get<Resource>(id);
}

template <typename Resource>
auto gse::renderer::system::get(const std::string& filename) -> resource::handle<Resource> {
	return m_context->get<Resource>(filename);
}

template <typename Resource, typename ... Args>
auto gse::renderer::system::queue(const std::string& name, Args&&... args) -> resource::handle<Resource> {
	return m_context->queue<Resource>(name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::renderer::system::instantly_load(const id& id) -> resource::handle<Resource> {
	return m_context->instantly_load<Resource>(id);
}

template <typename Resource>
auto gse::renderer::system::add(Resource&& resource) -> void {
	m_context->add<Resource>(std::forward<Resource>(resource));
}

template <typename Resource>
auto gse::renderer::system::resource_state(const id& id) const -> resource::state {
	return m_context->resource_state<Resource>(id);
}
