export module gs:balance;

import std;
import gse;

export namespace gs::balance {
	struct component {
		gse::id support_a;
		gse::id support_b;
		gse::time response_time = gse::seconds(0.25f);
		gse::time pendulum_time = gse::seconds(0.32f);
		float damping = 1.5f;
		gse::velocity max_correction = gse::meters_per_second(5.0f);
		gse::vec3<gse::velocity> correction_velocity;
		gse::displacement stepping_bias_forward;
	};

	struct system {
		struct data {
			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(gse::run_context& ctx, data& d) -> gse::async::task<>;
	};
}

auto gs::balance::system::run(gse::run_context& ctx, data& d) -> gse::async::task<> {
	while (true) {
		{
			auto [balances, transforms, motions] = co_await ctx.acquire_with(
				gse::write_v<component>,
				gse::read_v<gse::physics::transform_component>,
				gse::read_v<gse::physics::motion_component>
			);

			const bool log_now = d.log_timer.tick();

			const auto owner_ids = balances.owner_ids();
			for (std::size_t i = 0; i < balances.size(); ++i) {
				auto& b = balances[i];
				const auto owner = owner_ids[i];

				const auto* body_tc = transforms.find(owner);
				const auto* body_mc = motions.find(owner);
				const auto* support_a_tc = transforms.find(b.support_a);
				const auto* support_b_tc = transforms.find(b.support_b);

				if (!body_tc || !body_mc || !support_a_tc || !support_b_tc) {
					b.correction_velocity = {};
					if (log_now) {
						gse::log::println(
							"balance owner={} missing: body_tc={} body_mc={} support_a={} support_b={}",
							owner.number(),
							body_tc != nullptr,
							body_mc != nullptr,
							support_a_tc != nullptr,
							support_b_tc != nullptr
						);
					}
					continue;
				}

				const auto support_center =
					support_a_tc->position + (support_b_tc->position - support_a_tc->position) * 0.5f;
				const auto offset = body_tc->position - support_center;
				const auto lean = gse::vec3<gse::displacement>(offset.x(), gse::meters(0.f), offset.z());
				const auto horizontal_v = gse::vec3<gse::velocity>(
					body_mc->current_velocity.x(),
					gse::meters_per_second(0.f),
					body_mc->current_velocity.z()
				);

				auto correction = -lean / b.response_time - horizontal_v * b.damping;
				const auto correction_mag = gse::magnitude(correction);
				if (correction_mag > b.max_correction) {
					correction = correction * (b.max_correction / correction_mag);
				}
				b.correction_velocity = correction;

				const auto capture_point_world = lean + horizontal_v * b.pendulum_time;
				const auto capture_point_body =
					gse::rotate_vector(gse::conjugate(body_tc->orientation), capture_point_world);
				b.stepping_bias_forward = -capture_point_body.z();

				if (log_now) {
					gse::log::println(
						"balance owner={} pelvis=({:+.2f},{:+.2f},{:+.2f}) feet_ctr=({:+.2f},{:+.2f},{:+.2f}) "
						"lean=({:+.3f},{:+.3f}) vel=({:+.2f},{:+.2f}) correction=({:+.2f},{:+.2f}) |c|={:.2f} "
						"step_bias_fwd={:+.3f}",
						owner.number(),
						static_cast<float>(body_tc->position.x()),
						static_cast<float>(body_tc->position.y()),
						static_cast<float>(body_tc->position.z()),
						static_cast<float>(support_center.x()),
						static_cast<float>(support_center.y()),
						static_cast<float>(support_center.z()),
						static_cast<float>(lean.x()),
						static_cast<float>(lean.z()),
						static_cast<float>(horizontal_v.x()),
						static_cast<float>(horizontal_v.z()),
						static_cast<float>(correction.x()),
						static_cast<float>(correction.z()),
						static_cast<float>(gse::magnitude(correction)),
						static_cast<float>(b.stepping_bias_forward)
					);
				}
			}
		}

		co_await ctx.next_tick();
	}
}
