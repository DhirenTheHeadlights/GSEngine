export module gs:piston;

import std;
import gse;

export namespace gs::piston {
	struct component {
		gse::vec3<gse::position> center;
		gse::vec3<gse::length> amplitude;
		gse::angular_velocity omega = gse::radians_per_second(0.f);
		gse::angle phase = gse::radians(0.f);
	};

	struct system {
		static auto run(
			gse::run_context& ctx
		) -> gse::async::task<>;
	};
}

auto gs::piston::system::run(gse::run_context& ctx) -> gse::async::task<> {
	while (true) {
		{
			auto [pistons, transforms, motions] = co_await ctx.acquire_with(
				gse::write_v<component>,
				gse::write_v<gse::physics::transform_component>,
				gse::write_v<gse::physics::motion_component>
			);

			const int steps = gse::system_clock::fixed_steps_this_frame();
			const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
			const float frame_step_count = static_cast<float>(steps);

			const auto piston_ids = pistons.owner_ids();
			for (std::size_t i = 0; i < pistons.size(); ++i) {
				auto& p = pistons[i];
				const auto eid = piston_ids[i];
				auto* motion = motions.find(eid);
				auto* transform = transforms.find(eid);
				if (!motion || !transform) {
					continue;
				}

				const float s = gse::sin(p.phase);
				const float c = gse::cos(p.phase);

				transform->position = p.center + p.amplitude * s;
				motion->current_velocity = p.amplitude * c * p.omega / gse::rad;
				motion->angular_velocity = {};

				p.phase += p.omega * step_dt * frame_step_count;
			}
		}

		co_await ctx.next_tick();
	}
}
