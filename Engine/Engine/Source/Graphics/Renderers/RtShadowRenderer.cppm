export module gse.graphics:rt_shadow_renderer;

import std;

import :geometry_collector;
import :grass_renderer;
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

export namespace gse::renderer::rt_shadow {
	using blas_key = std::pair<id, std::uint32_t>;

	constexpr std::uint32_t grass_tlas_capacity = 4096;
	constexpr std::uint32_t grass_static_batch = 64;
	constexpr std::uint32_t grass_dynamic_growth = 16;
	constexpr gpu::device_size grass_static_budget = gpu::device_size{ 2 } * 1024 * 1024 * 1024;

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

	struct grass_occupied_slot {
		std::uint32_t slot = 0;
		std::uint32_t tile_plus_one = 0;
	};

	struct grass_mesh_pass {};

	struct grass_rt_field {
		physics::grass_field field;
		std::uint32_t tile_count = 0;
		bool over_budget = false;
		std::vector<gpu::blas> static_blas;
		std::uint32_t static_built = 0;
		per_frame_resource<std::uint32_t> static_published;
		per_frame_resource<gpu::buffer> static_addresses;
		per_frame_resource<gpu::buffer> static_vertices;
		per_frame_resource<std::vector<gpu::blas>> dynamic_blas;
		per_frame_resource<std::vector<std::uint32_t>> dynamic_built_tile;
		per_frame_resource<gpu::buffer> dynamic_vertices;
		per_frame_resource<std::uint32_t> dynamic_capacity;
		per_frame_resource<gpu::buffer> dynamic_addresses;
		gpu::readback_channel slot_readback;
		std::uint64_t slot_generation = 0;
		std::uint64_t slot_applied = 0;
		std::vector<grass_occupied_slot> occupied;
	};

	struct [[= system_state<"RtShadow">{}]] data {
		std::flat_map<blas_key, blas_entry> blas_cache;
		std::flat_map<blas_key, per_frame_resource<deformed_blas_entry>> deformed_blas;
		per_frame_resource<gpu::buffer> blas_scratch;
		per_frame_resource<gpu::tlas> tlas_per_frame;
		per_frame_resource<linear_vector<gpu::tlas_instance_desc>> instances;

		gpu::shader_program tlas_update_pipeline;
		per_frame_resource<gpu::buffer> mapping_buffers;
		std::size_t mapping_buffer_capacity = 0;

		per_frame_resource<gpu::bindless_handle> tlas_instance_views;
		per_frame_resource<gpu::bindless_handle> tlas_slots;

		gpu::shader_program grass_mesh_pipeline;
		gpu::shader_program grass_instance_pipeline;
		gpu::device_size grass_tile_blas_bytes = 0;
		gpu::device_size grass_tile_scratch = 0;
		per_frame_resource<gpu::buffer> grass_scratch;
		std::flat_map<id, grass_rt_field> grass_fields;
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
		shared_view<grass::data> grass_r,
		shared_view<physics::grass::data> grass_s
	) -> async::task<>;
}
