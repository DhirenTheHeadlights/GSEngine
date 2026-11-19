export module sandbox:joints;

import std;
import gse;

import :archetypes;

export namespace sandbox {
	auto spawn_fixed_joint(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_distance_pendulum(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_pendulum_chain(
		gse::scene& s,
		int index,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_spring_tests(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;
}

auto sandbox::spawn_fixed_joint(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto anchor_pos = origin + gse::meters(0.f, 5.f, 0.f);
	const auto anchor_id = s.spawn("Fixed Anchor", static_box(anchor_pos, gse::vec3<gse::length>(gse::meters(1.f))));

	const auto hanging_id = s.spawn(
		"Fixed Hanging Box",
		box(
			anchor_pos + gse::meters(0.f, -1.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(20.f)
		)
	);

	s.build("Fixed Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = anchor_id,
			.entity_b = hanging_id,
			.config = gse::physics::fixed_joint{
				.anchor_a = gse::vec3<gse::displacement>(gse::meters(0.f, -0.5f, 0.f)),
				.anchor_b = gse::vec3<gse::displacement>(gse::meters(0.f, 0.5f, 0.f)),
			},
		});
}

auto sandbox::spawn_distance_pendulum(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto pivot_pos = origin + gse::meters(0.f, 8.f, 0.f);
	const auto pivot_id = s.spawn("Distance Pivot", static_box(pivot_pos, gse::meters(0.5f, 0.5f, 0.5f)));

	const auto bob_id = s.spawn(
		"Distance Bob",
		box(
			pivot_pos + gse::meters(3.f, -3.f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(30.f)
		)
	);

	s.build("Distance Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = pivot_id,
			.entity_b = bob_id,
			.config = gse::physics::distance_joint{
				.target = gse::meters(4.f),
			},
		});
}

auto sandbox::spawn_pendulum_chain(gse::scene& s, const int index, const gse::vec3<gse::position>& origin) -> void {
	constexpr int chain_length = 5;
	constexpr float link_spacing = 1.5f;

	const auto ceiling_pos = origin + gse::meters(0.f, 12.f, 0.f);
	const auto ceiling_id = s.spawn(std::format("Chain {} Ceiling", index), static_box(ceiling_pos, gse::meters(1.f, 0.5f, 1.f)));

	std::vector<gse::id> link_ids;
	link_ids.push_back(ceiling_id);

	for (int i = 0; i < chain_length; ++i) {
		const float y = 12.f - (static_cast<float>(i) + 1.f) * link_spacing;
		const float x_offset = (i == chain_length - 1) ? 2.f : 0.f;
		const auto link_pos = origin + gse::meters(x_offset, y, 0.f);
		const auto link_id = s.spawn(
			std::format("Chain {} Link {}", index, i),
			box(
				link_pos,
				gse::meters(0.6f, 0.6f, 0.6f),
				gse::kilograms(15.f)
			)
		);
		link_ids.push_back(link_id);
	}

	for (std::size_t i = 0; i + 1 < link_ids.size(); ++i) {
		s.build(std::format("Chain {} Joint {}", index, i))
			.with<gse::physics::joint_spec>({
				.entity_a = link_ids[i],
				.entity_b = link_ids[i + 1],
				.config = gse::physics::distance_joint{
					.target = gse::meters(link_spacing),
				},
			});
	}
}

auto sandbox::spawn_spring_tests(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr std::array compliances = {
		gse::per_kilograms(0.001f),
		gse::per_kilograms(0.01f),
		gse::per_kilograms(0.1f),
	};

	for (int i = 0; i < 3; ++i) {
		const std::array<std::string, 3> labels = { "Stiff", "Medium", "Soft" };
		const auto anchor_pos = origin + gse::meters(static_cast<float>(i) * 5.f, 10.f, 0.f);

		const auto anchor_id = s.spawn(
			std::format("Spring {} Anchor", labels[i]),
			static_box(anchor_pos, gse::meters(0.5f, 0.5f, 0.5f))
		);

		const auto bob_id = s.spawn(
			std::format("Spring {} Bob", labels[i]),
			sphere(
				anchor_pos + gse::meters(2.f, 0.f, 0.f),
				gse::meters(0.5f),
				gse::sphere_lod::lo
			)
		);

		s.build(std::format("Spring {} Joint", labels[i]))
			.with<gse::physics::joint_spec>({
				.entity_a = anchor_id,
				.entity_b = bob_id,
				.config = gse::physics::spring_joint{
					.target = gse::meters(4.f),
					.compliance = compliances[i],
					.damping = 0.3f,
				},
			});
	}

	const auto chain_anchor_pos = origin + gse::meters(18.f, 12.f, 0.f);
	const auto chain_anchor =
		s.spawn(
			"Spring Chain Anchor",
			static_box(chain_anchor_pos, gse::meters(0.5f, 0.5f, 0.5f))
		);

	auto prev_id = chain_anchor;
	for (int i = 0; i < 5; ++i) {
		const auto link_pos = chain_anchor_pos + gse::meters(0.f, -static_cast<float>(i + 1) * 2.f, 0.f);
		const auto link_id = s.spawn(
			std::format("Spring Chain Link {}", i),
			sphere(link_pos, gse::meters(0.4f), gse::sphere_lod::lo)
		);

		s.build(std::format("Spring Chain Joint {}", i))
			.with<gse::physics::joint_spec>({
				.entity_a = prev_id,
				.entity_b = link_id,
				.config = gse::physics::spring_joint{
					.target = gse::meters(1.5f),
					.compliance = gse::per_kilograms(0.02f),
					.damping = 0.5f,
				},
			});
		prev_id = link_id;
	}
}
