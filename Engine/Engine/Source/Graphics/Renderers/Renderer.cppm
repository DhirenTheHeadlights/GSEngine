export module gse.graphics:renderer;

import std;

import :clip;
import :render_component;
import :material;
import :font;
import :model;
import :skinned_model;
import :skeleton;
import :texture;

import :texture_compiler;
import :font_compiler;
import :model_compiler;
import :material_compiler;
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
		bool frame_begun = false;
		bool hot_reload_enabled = false;
		vec2f last_viewport{ 1920.f, 1080.f };

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
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto update(const update_phase& phase, state& s) -> void;
		static auto begin_frame(const begin_frame_phase& phase, state& s) -> bool;
		static auto end_frame(const end_frame_phase& phase, state& s) -> void;
		static auto shutdown(const shutdown_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::system::initialize(const initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	s.ctx = &ctx;

	ctx.add_loader<texture>();
	ctx.add_loader<model>();
	ctx.add_loader<skinned_model>();
	ctx.add_loader<font>();
	ctx.add_loader<material>();
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

auto gse::renderer::system::update(const update_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

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
	const auto new_viewport = vec2f(
		static_cast<float>(window_size.x()),
		static_cast<float>(window_size.y())
	);

	if (new_viewport.x() != s.last_viewport.x() || new_viewport.y() != s.last_viewport.y()) {
		phase.channels.push(camera::viewport_update{ .size = new_viewport });
		s.last_viewport = new_viewport;
	}
}

auto gse::renderer::system::begin_frame(const begin_frame_phase& phase, state& s) -> bool {
	auto& ctx = phase.get<gpu::context>();

	ctx.process_gpu_queue();

	clock fence_timer;
	auto result = ctx.begin_frame();
	const auto fence_wait = fence_timer.elapsed();
	s.frame_begun = result.has_value();

	if (fence_wait > milliseconds(4.f)) {
		log::println(log::level::warning, log::category::vulkan, "begin_frame fence stall: {}ms", fence_wait.as<milliseconds>());
	}

	if (!result && result.error() == gpu::frame_status::device_lost) {
		log::println(log::level::error, log::category::vulkan, "Device lost during begin_frame — terminating");
		std::abort();
	}

	return s.frame_begun;
}

auto gse::renderer::system::end_frame(const end_frame_phase& phase, state& s) -> void {
	if (!s.frame_begun) {
		return;
	}

	auto& ctx = phase.get<gpu::context>();

	ctx.end_frame();

	ctx.finalize_reloads();

	s.frame_begun = false;
}

auto gse::renderer::system::shutdown(const shutdown_phase& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	ctx.wait_idle();
	ctx.shutdown();
}
