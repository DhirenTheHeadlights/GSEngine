export module gse.graphics:renderer;

import std;

import :clip;
import :render_component;
import :font;
import :model;
import :skinned_model;
import :skeleton;
import :texture;

import :texture_compiler;
import :font_compiler;
import :skeleton_compiler;
import :clip_compiler;
import :skinned_model_compiler;
import :camera_system;

import gse.log;
import gse.utility;
import gse.platform;
import gse.audio;
import gse.math;

export namespace gse::renderer {
	struct state {
		gpu::context* ctx = nullptr;
		bool hot_reload_enabled = false;
		vec2f last_viewport{ 1920.f, 1080.f };

		state() = default;

		auto set_ui_focus(const bool focus) const -> void {
			ctx->set_ui_focus(focus);
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
		auto queue(const std::string& name, Args&&... args) const -> resource::handle<Resource> {
			return ctx->queue<Resource>(name, std::forward<Args>(args)...);
		}

		template <typename Resource>
		auto instantly_load(const id& resource_id) const -> resource::handle<Resource> {
			return ctx->instantly_load<Resource>(resource_id);
		}

		template <typename Resource>
		auto add(Resource&& resource) const -> void {
			ctx->add<Resource>(std::forward<Resource>(resource));
		}

		template <typename Resource>
		auto resource_state(const id& resource_id) const -> resource::state {
			return ctx->resource_state<Resource>(resource_id);
		}

		auto context_ref() -> gpu::context& {
			return *ctx;
		}

		auto context_ref() const -> const gpu::context& {
			return *ctx;
		}
	};

	struct system {
		static auto initialize(const init_context& phase, state& s) -> void;
		static auto update(const update_context& ctx, state& s) -> void;
		static auto shutdown(const shutdown_context& phase, const state& s) -> void;
	};
}

auto gse::renderer::system::initialize(const init_context& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	s.ctx = &ctx;

	ctx.add_loader<texture>();
	ctx.add_loader<model>();
	ctx.add_loader<skinned_model>();
	ctx.add_loader<font>();
	ctx.add_loader<skeleton>();
	ctx.add_loader<clip_asset>();
	ctx.add_loader<audio_clip>();

	ctx.compile();

	phase.channels.push(save::register_property{
		.category = "Graphics",
		.name = "Hot Reload",
		.description = "Automatically reload assets when source files change",
		.ref = &s.hot_reload_enabled,
		.type = typeid(bool)
	});
}

auto gse::renderer::system::update(const update_context& ctx, state& s) -> void {
	auto& gpu = ctx.get<gpu::context>();

	if (s.hot_reload_enabled != gpu.hot_reload_enabled()) {
		if (s.hot_reload_enabled) {
			gpu.enable_hot_reload();
		} else {
			gpu.disable_hot_reload();
		}
	}

	gpu.poll_assets();
	gpu.process_resource_queue();
	gpu.window().update(gpu.ui_focus());

	ctx.channels.push(camera::ui_focus_update{ .focus = gpu.ui_focus() });

	const auto window_size = gpu.window().viewport();
	const auto new_viewport = vec2f(
		static_cast<float>(window_size.x()),
		static_cast<float>(window_size.y())
	);

	if (new_viewport.x() != s.last_viewport.x() || new_viewport.y() != s.last_viewport.y()) {
		ctx.channels.push(camera::viewport_update{ .size = new_viewport });
		s.last_viewport = new_viewport;
	}
}

auto gse::renderer::system::shutdown(const shutdown_context& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	ctx.wait_idle();
	ctx.shutdown();
}
