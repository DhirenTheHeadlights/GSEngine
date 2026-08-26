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

import :motion_component;
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
	};

	struct pending_step {
		std::uint64_t generation = 0;
		std::uint32_t step = 0;
		bool cpu_recorded = false;
		std::vector<body_sample> pre;
		std::vector<body_sample> cpu_post;
	};

	struct [[= system_state<"Shadow Step">{}, = settings::category<"ShadowStep">{}, = deferred_system{}]] data {
		[[
			= settings::describe<"Step a second, GPU-resident copy of the solver from the CPU solver's pre-step state "
									  "every tick and log the one-step residual, then discard it. A parity instrument: it "
									  "denies divergence any time to amplify, so a residual is a defect rather than chaos. "
									  "Stalls the device once per tick, so timings from such a run are meaningless. "
									  "Requires a restart, and requires the CPU solver.">{},
			= settings::restart_required{}
		]]
		bool enabled = false;

		[[
			= settings::describe<"Dump every body's one-step residual at this shadow step, once; -1 dumps nothing. "
									  "Choose the step from the aggregate column rather than letting a magnitude trigger "
									  "choose it: the steps just after first contact carry a large backend-independent "
									  "residual from the per-tick reseed, so any threshold fires there instead of at the "
									  "step where the backends actually part company.">{}
		]]
		int detail_step = -1;

		std::uint32_t step_index = 0;
		bool buffers_ready = false;
		bool conflict_reported = false;
		std::vector<pending_step> pending;
		vbd::gpu_solver solver;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_run<1>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys,
		read<transform_component> transform,
		read<motion_component> motion,
		read<collision_component> collision
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		std::optional<shared_view<gpu::context::data>> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out
	) -> async::task<>;
}