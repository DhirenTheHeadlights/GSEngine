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

import :capture_renderer;
import :cull_compute_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :geometry_collector;
import :light_culling_renderer;
import :physics_debug_renderer;
import :physics_transform_renderer;
import :rt_shadow_renderer;
import :skin_compute_renderer;
import :ui_renderer;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.audio;
import gse.math;
import gse.save;

export namespace gse::renderer {
	struct state {
		bool hot_reload_enabled = false;
		bool gpu_timestamps_enabled = true;
		bool gpu_pipeline_stats_enabled = false;
		bool profile_aggregator_enabled = true;
		actions::handle dump_profile_action;
		vec2f last_viewport{ 1920.f, 1080.f };
	};

	struct system {
		struct resources {
			gpu::context* ctx = nullptr;

			auto set_ui_focus(
				bool focus
			) const -> void;

			template <typename Resource>
			auto get(
				const id& resource_id
			) const -> resource::handle<Resource>;

			template <typename Resource>
			auto get(
				const std::string& filename
			) const -> resource::handle<Resource>;

			template <typename Resource>
			auto try_get(
				const id& resource_id
			) const -> resource::handle<Resource>;

			template <typename Resource>
			auto try_get(
				const std::string& filename
			) const -> resource::handle<Resource>;

			template <typename Resource, typename... Args>
			auto queue(
				const std::string& name,
				Args&&... args
			) const -> resource::handle<Resource>;

			template <typename Resource>
			auto instantly_load(
				const id& resource_id
			) const -> resource::handle<Resource>;

			template <typename Resource>
			auto add(
				Resource&& resource
			) const -> void;

			template <typename Resource>
			auto resource_state(
				const id& resource_id
			) const -> resource::state;

			auto context_ref(
			) -> gpu::context&;

			auto context_ref(
			) const -> const gpu::context&;
		};

		static auto initialize(const init_context& phase, resources& r, state& s) -> void;
		static auto update(const update_context& ctx, state& s) -> async::task<>;
		static auto shutdown(const shutdown_context& phase) -> void;
	};
}

template <typename Resource>
auto gse::renderer::system::resources::get(const id& resource_id) const -> resource::handle<Resource> {
	return ctx->get<Resource>(resource_id);
}

template <typename Resource>
auto gse::renderer::system::resources::get(const std::string& filename) const -> resource::handle<Resource> {
	return ctx->get<Resource>(filename);
}

template <typename Resource>
auto gse::renderer::system::resources::try_get(const id& resource_id) const -> resource::handle<Resource> {
	return ctx->try_get<Resource>(resource_id);
}

template <typename Resource>
auto gse::renderer::system::resources::try_get(const std::string& filename) const -> resource::handle<Resource> {
	return ctx->try_get<Resource>(filename);
}

template <typename Resource, typename... Args>
auto gse::renderer::system::resources::queue(const std::string& name, Args&&... args) const -> resource::handle<Resource> {
	return ctx->queue<Resource>(name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::renderer::system::resources::instantly_load(const id& resource_id) const -> resource::handle<Resource> {
	return ctx->instantly_load<Resource>(resource_id);
}

template <typename Resource>
auto gse::renderer::system::resources::add(Resource&& resource) const -> void {
	ctx->add<Resource>(std::forward<Resource>(resource));
}

template <typename Resource>
auto gse::renderer::system::resources::resource_state(const id& resource_id) const -> resource::state {
	return ctx->resource_state<Resource>(resource_id);
}
