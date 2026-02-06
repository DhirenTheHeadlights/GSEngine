export module gse.graphics:renderer;

import std;

import :clip;
import :render_component;
import :resource_loader;
import :material;
import :rendering_context;
import :font;
import :model;
import :skinned_model;
import :shader;
import :skeleton;
import :texture;

import :shader_compiler;
import :shader_layout_compiler;
import :texture_compiler;
import :font_compiler;
import :model_compiler;
import :material_compiler;
import :skeleton_compiler;
import :clip_compiler;
import :skinned_model_compiler;
import :camera_system;

import gse.utility;
import gse.platform;

export namespace gse::renderer {
	struct state {
		context* ctx = nullptr;
		bool frame_begun = false;
		bool hot_reload_enabled = false;
		unitless::vec2 last_viewport{ 1920.f, 1080.f };

		explicit state(context& c) : ctx(std::addressof(c)) {}
		state() = default;

		auto set_ui_focus(const bool focus) const -> void {
			ctx->set_ui_focus(focus);
		}

		auto is_frame_begun() const -> bool {
			return frame_begun;
		}

		template <typename Resource>
		auto get(const id& resource_id) const -> resource::handle<Resource> {
			return ctx->get<Resource>(resource_id);
		}

		template <typename Resource>
		auto get(const std::string& filename) const -> resource::handle<Resource> {
			return ctx->get<Resource>(filename);
		}

		template <typename Resource>
		auto try_get(const id& resource_id) const -> resource::handle<Resource> {
			return ctx->try_get<Resource>(resource_id);
		}

		template <typename Resource>
		auto try_get(const std::string& filename) const -> resource::handle<Resource> {
			return ctx->try_get<Resource>(filename);
		}

		template <typename Resource, typename... Args>
		auto queue(const std::string& name, Args&&... args) -> resource::handle<Resource> {
			return ctx->queue<Resource>(name, std::forward<Args>(args)...);
		}

		template <typename Resource>
		auto instantly_load(const id& resource_id) -> resource::handle<Resource> {
			return ctx->instantly_load<Resource>(resource_id);
		}

		template <typename Resource>
		auto add(Resource&& resource) -> void {
			ctx->add<Resource>(std::forward<Resource>(resource));
		}

		template <typename Resource>
		auto resource_state(const id& resource_id) const -> resource::state {
			return ctx->resource_state<Resource>(resource_id);
		}

		auto context_ref() -> context& {
			return *ctx;
		}

		auto context_ref() const -> const context& {
			return *ctx;
		}
	};

	struct system {
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto begin_frame(begin_frame_phase& phase, state& s) -> bool;
		static auto end_frame(end_frame_phase& phase, state& s) -> void;
		static auto shutdown(shutdown_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::system::initialize(const initialize_phase& phase, state& s) -> void {
	auto& ctx = *s.ctx;

	ctx.add_loader<texture>();
	ctx.add_loader<model>();
	ctx.add_loader<skinned_model>();
	ctx.add_loader<shader>();
	ctx.add_loader<font>();
	ctx.add_loader<material>();
	ctx.add_loader<skeleton>();
	ctx.add_loader<clip_asset>();

	ctx.compile();

	if (const auto* save_state = phase.try_state_of<save::state>()) {
		s.hot_reload_enabled = save_state->read("Graphics", "Hot Reload", false);
	}

	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "Hot Reload",
		.description = "Automatically reload assets when source files change",
		.ref = &s.hot_reload_enabled,
		.type = typeid(bool)
	});
}

auto gse::renderer::system::update(update_phase& phase, state& s) -> void {
	auto& ctx = *s.ctx;

	if (s.hot_reload_enabled != ctx.hot_reload_enabled()) {
		if (s.hot_reload_enabled) {
			ctx.enable_hot_reload();
		} else {
			ctx.disable_hot_reload();
		}
	}

	ctx.poll_assets();
	ctx.process_resource_queue();
	ctx.window().update(ctx.ui_focus());

	phase.channels.push(camera::ui_focus_update{ .focus = ctx.ui_focus() });

	const auto window_size = ctx.window().viewport();
	const auto new_viewport = unitless::vec2(
		static_cast<float>(window_size.x()),
		static_cast<float>(window_size.y())
	);

	if (new_viewport.x() != s.last_viewport.x() || new_viewport.y() != s.last_viewport.y()) {
		phase.channels.push(camera::viewport_update{ .size = new_viewport });
		s.last_viewport = new_viewport;
	}
}

auto gse::renderer::system::begin_frame(begin_frame_phase&, state& s) -> bool {
	auto& ctx = *s.ctx;

	ctx.process_gpu_queue();

	s.frame_begun = vulkan::begin_frame({
		.window = ctx.window().raw_handle(),
		.frame_buffer_resized = ctx.window().frame_buffer_resized(),
		.minimized = ctx.window().minimized(),
		.config = ctx.config()
	});

	return s.frame_begun;
}

auto gse::renderer::system::end_frame(end_frame_phase&, state& s) -> void {
	if (!s.frame_begun) {
		return;
	}

	auto& ctx = *s.ctx;

	vulkan::end_frame({
		.window = ctx.window().raw_handle(),
		.frame_buffer_resized = ctx.window().frame_buffer_resized(),
		.minimized = ctx.window().minimized(),
		.config = ctx.config()
	});

	ctx.finalize_reloads();

	s.frame_begun = false;
}

auto gse::renderer::system::shutdown(shutdown_phase&, const state& s) -> void {
	auto& ctx = *s.ctx;

	ctx.config().device_config().device.waitIdle();
	ctx.shutdown();
}
