export module gse.physics:vbd_shadow_step;

import std;

import gse.core;
import gse.math;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.ecs;
import gse.log;
import gse.gpu;
import gse.gpu_record;

import :joint_drive_component;
import :motion_component;
import :motor_component;
import :muscle_component;
import :collision_component;
import :transform_component;
import :system;
import :vbd_gpu_solver;

export namespace gse::physics::shadow_step {
	struct body_sample {
		id owner;
		std::uint32_t body_index = 0;
		vec3<position> position;
		vec3<velocity> velocity;
		vec3<angular_velocity> spin;
	};

	struct pending_step {
		std::uint64_t generation = 0;
		std::uint32_t step = 0;
		bool cpu_recorded = false;
		std::vector<body_sample> pre;
		std::vector<body_sample> cpu_post;
	};

	struct [[= system_state<"Shadow Step">{}, = deferred_system{}]] data {
		bool enabled = false;
		int detail_step = -1;
		std::uint32_t step_index = 0;
		bool conflict_reported = false;
		std::vector<pending_step> pending;
		std::vector<joint_definition> joints;
		std::vector<muscle_definition> muscles;
		std::vector<std::uint32_t> hull_data;
		std::vector<std::uint32_t> hull_offsets;
		std::size_t packed_hull_count = 0;
		std::uint64_t hull_generation = 0;
		std::vector<std::pair<std::uint32_t, std::uint32_t>> non_colliding_pairs;
		vbd::gpu_solver solver;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		shared_view<physics::data> phys,
		data& d
	) -> async::task<>;

	[[= system_run<1>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys,
		read<transform_component> transform,
		read<motion_component> motion,
		read<collision_component> collision,
		read<motor_component> motor,
		read<joint_drive_component> drives,
		read<muscle_component> muscles
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;
}