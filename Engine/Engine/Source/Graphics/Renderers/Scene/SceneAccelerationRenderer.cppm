export module gse.graphics:scene_acceleration_renderer;

import std;

import :geometry_collector;
import :lighting_source;
import :mesh;
import :skin_renderer;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.log;
import gse.gpu_record;
import gse.physics;

export namespace gse::renderer::scene_acceleration {
	using blas_key = std::pair<id, std::uint32_t>;

	constexpr std::uint32_t contributed_instance_capacity = 4096;

	struct instance_reservation {
		id owner;
		std::uint32_t capacity = 0;
	};

	struct instance_range {
		std::uint32_t first = 0;
		std::uint32_t capacity = 0;
	};

	struct instances_pass {};

	struct blas_entry {
		const mesh* source = nullptr;
		gpu::blas blas;
	};

	struct deformed_blas_entry {
		gpu::blas blas;
		const mesh* source = nullptr;
		std::uint32_t vertex_count = 0;
		std::uint32_t prim_count = 0;
		gpu::device_size scratch_size = 0;
		bool built = false;
	};

	struct [[= system_state<"SceneAcceleration">{}, = requires_setting<^^lighting::data::lighting_technique, lighting::technique::ray_traced>{}]] data {
		std::flat_map<blas_key, blas_entry> blas_cache;
		std::flat_map<blas_key, per_frame_resource<deformed_blas_entry>> deformed_blas;
		per_frame_resource<gpu::buffer> blas_scratch;
		per_frame_resource<gpu::tlas> tlas_per_frame;
		per_frame_resource<linear_vector<gpu::tlas_instance_desc>> instances;

		gpu::shader_program tlas_update_pipeline;
		per_frame_resource<gpu::buffer> mapping_buffers;
		std::size_t mapping_buffer_capacity = 0;

		[[= shared]] per_frame_resource<gpu::bindless_handle> tlas_instance_views;
		per_frame_resource<gpu::bindless_handle> tlas_slots;

		std::flat_map<id, std::uint32_t> reservations;
		[[= shared]] std::flat_map<id, instance_range> contributed;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_write<lighting_source::ray_traced_scene> scene_out,
		channel_read<geometry_collector::render_data> geometry_in,
		shared_view<geometry_collector::data> gc_r,
		shared_view<skin::data> skin_s,
		channel_read<instance_reservation> reservations_in
	) -> async::task<>;
}
