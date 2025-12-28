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
	private:
		context* m_context = nullptr;
		bool m_frame_begun = false;
	};
}

gse::renderer::system::system(renderer::context& context) : m_context(std::addressof(context)) {
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
		ctx.camera().process_mouse_movement(mouse::delta());
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
