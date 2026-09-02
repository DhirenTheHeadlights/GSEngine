export module sandbox:runtime_spawns;

import std;
import gse;

import :character_controller;
import :entity_builders;
import :orbit_camera;
import :piston;
import :sidearm;
import :tumbler;

export namespace sandbox {
	struct stress_scene_params {
		[[
			= gse::settings::describe<"Cubes along each radial axis of a tumbler drum. The drum geometry scales with "
									  "the cube counts at a fixed pitch, so this is the scene's main size dial; the "
									  "two drums hold the bulk of the stress scene's bodies. At the slider maxima the "
									  "pair approaches the solver's body capacity.">{},
			= gse::settings::range<1, 16>{}
		]]
		int tumbler_radial_cubes = 12;

		[[
			= gse::settings::describe<"Cubes along the axial length of a tumbler drum. The drum length scales to "
									  "match.">{},
			= gse::settings::range<1, 32>{}
		]]
		int tumbler_axial_cubes = 24;

		[[
			= gse::settings::describe<"Boxes per side of the stacked box grid.">{},
			= gse::settings::range<1, 16>{}
		]]
		int box_grid_side = 6;

		[[
			= gse::settings::describe<"Layers in the stacked box grid.">{},
			= gse::settings::range<1, 8>{}
		]]
		int box_grid_layers = 3;
	};

	struct character_spawn_params {
		[[
			= gse::settings::describe<"Characters built by one spawn request, whether it came from F7 or the locomotion "
									  "scenario. F7 wants the one character it possesses, so the locomotion scenario pins "
									  "its own higher count instead: a single character measures close to the noise floor, "
									  "and a regression threshold needs several before it means anything.">{},
			= gse::settings::range<1, 32>{}
		]]
		int count = 1;
	};

	struct pyramid_scene_params {
		[[
			= gse::settings::describe<"Blocks along the base of the pyramid workload. The stack is this many rows tall and "
									  "holds base_count * (base_count + 1) / 2 blocks, so this is the dial that sets how "
									  "much load the bottom contacts carry.">{},
			= gse::settings::range<1, 200>{}
		]]
		int base_count = 50;
	};

	struct light_field_params {
		[[
			= gse::settings::describe<"How many clusters of point lights a light-field spawn builds. Clustering is what "
									  "loads the tiled light culling pass unevenly, so neighbouring tiles hold wildly "
									  "different light counts instead of all holding the same few.">{},
			= gse::settings::range<1, 4096>{}
		]]
		int clusters = 384;

		[[
			= gse::settings::describe<"How many point lights sit in each cluster. One light per cluster scatters the "
									  "field evenly; raising this concentrates the same budget into pockets.">{},
			= gse::settings::range<1, 64>{}
		]]
		int lights_per_cluster = 1;

		[[
			= gse::settings::describe<"Radius of the cylinder the clusters are scattered through.">{}
		]]
		gse::length field_radius = gse::meters(46.f);

		[[
			= gse::settings::describe<"Height of the cylinder the clusters are scattered through.">{}
		]]
		gse::length field_height = gse::meters(34.f);

		[[
			= gse::settings::describe<"Radius of the sphere each cluster's lights are scattered through.">{}
		]]
		gse::length cluster_radius = gse::meters(5.f);

		[[
			= gse::settings::describe<"Radius of each emissive sphere standing in for a light.">{}
		]]
		gse::length source_radius = gse::meters(0.35f);

		[[
			= gse::settings::describe<"Radiant intensity of each light.">{}
		]]
		gse::irradiance intensity = gse::watts_per_square_meter(26.f);

		[[
			= gse::settings::describe<"How far each light reaches before the culling pass cuts it off. Attenuation is "
									  "solved back from this so the radius holds as intensity changes. This is the dial "
									  "that must come down as density goes up: lights that all reach across the whole "
									  "scene push every tile past the per-tile light cap, and the culling result then "
									  "degrades into visible 16-pixel blocks.">{}
		]]
		gse::length falloff_radius = gse::meters(25.f);

		[[
			= gse::settings::describe<"How far each light drifts from where it spawned. Zero leaves the field static; "
									  "any larger value makes the lights kinematic so the culling pass has to re-derive "
									  "its tile lists every frame.">{}
		]]
		gse::length drift = gse::meters(0.f);

		[[
			= gse::settings::describe<"How fast each light travels around its drift cycle.">{}
		]]
		gse::angular_velocity drift_rate = gse::degrees_per_second(48.f);

		[[
			= gse::settings::describe<"Sun elevation held while the light hall is the active scene. The hall is a night "
									  "scene, so this sits below the horizon and the field is what lights it.">{},
			= gse::settings::range<-90.f, 90.f>{}
		]]
		gse::angle sun_elevation = gse::degrees(-9.f);
	};

	struct pyramid_strike_params {
		[[
			= gse::settings::describe<"First pyramid row the strike removes. Row 0 is the base; striking the base does "
									  "not collapse a pyramid, it just drops onto the floor intact.">{},
			= gse::settings::range<0, 200>{}
		]]
		int row_begin = 26;

		[[
			= gse::settings::describe<"How many rows the strike removes.">{},
			= gse::settings::range<1, 40>{}
		]]
		int rows = 6;

		[[
			= gse::settings::describe<"Fraction of each struck row to hit, measured from the left edge. Below 1.0 the "
									  "support is removed from one side only, so the section above topples instead of "
									  "dropping flat.">{},
			= gse::settings::range<0.f, 1.f>{}
		]]
		float width_fraction = 0.5f;

		[[
			= gse::settings::describe<"Impulse applied to each struck block.">{}
		]]
		gse::impulse strength = gse::newton_seconds(500.f);

		[[
			= gse::settings::describe<"Direction the struck blocks are pushed, in world space. Normalized before use.">{}
		]]
		gse::vec3f direction = { -1.f, 0.f, 0.f };
	};

	auto spawn_physics_stress(
		gse::scene& s,
		const stress_scene_params& params = {}
	) -> void;

	auto spawn_pyramid(
		gse::scene& s,
		const pyramid_scene_params& params = {}
	) -> void;

	auto spawn_light_field(
		gse::scene& s,
		const light_field_params& params = {}
	) -> void;

	auto spawn_joint_test(
		gse::scene& s
	) -> void;

	struct character_rig {
		gse::id character;
		gse::id proxy;
	};

	auto character_model(
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::resource::handle<gse::skinned_model>;

	auto character_clips(
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::animation::locomotion_blend;

	auto spawn_character(
		gse::scene& s,
		gse::id owner,
		const gse::resource::handle<gse::skinned_model>& model,
		const gse::animation::locomotion_blend& clips,
		const gse::vec3<gse::position>& origin
	) -> character_rig;

	auto possess_character(
		gse::scene& s,
		const character_rig& rig,
		const gse::animation::locomotion_blend& clips
	) -> void;

	auto spawn_tumbler(
		gse::scene& s,
		int index,
		const gse::vec3<gse::position>& center,
		const gse::vec3f& rotation_axis,
		gse::angular_velocity angular_speed,
		const stress_scene_params& params
	) -> void;
}

namespace sandbox {
	auto spawn_inverted_mass_pyramid(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_domino_chain(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_funnel(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_slope_friction_test(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_high_speed_impact_target(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_spring_tests(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_box_grid(
		gse::scene& s,
		const gse::vec3<gse::position>& origin,
		const stress_scene_params& params
	) -> void;

	auto spawn_fixed_joint(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_distance_pendulum(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_hinge_door(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_slider_elevator(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_pendulum_chain(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_sphere_stack(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_corner_drop(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_elevator(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;

	auto spawn_vise(
		gse::scene& s,
		const gse::vec3<gse::position>& origin
	) -> void;
}

auto sandbox::spawn_pyramid(gse::scene& s, const pyramid_scene_params& params) -> void {
	const int base_count = params.base_count;
	const gse::length extent = gse::meters(1.f);

	for (int row = 0; row < base_count; ++row) {
		const int count = base_count - row;
		const gse::length py = extent * (static_cast<float>(row) + 0.5f);

		for (int col = 0; col < count; ++col) {
			const gse::length px = extent * (static_cast<float>(col) - static_cast<float>(count - 1) * 0.5f);
			s.spawn(
				std::format("PyramidBlock_{}_{}", row, col),
				box(
					gse::vec3<gse::position>(px, py, gse::meters(0.f)),
					gse::vec3<gse::length>(extent),
					gse::kilograms(20.f)
				)
			);
		}
	}

	gse::log::println(
		"pyramid: base={} blocks={} height={:.1f:m}",
		base_count,
		base_count * (base_count + 1) / 2,
		extent * static_cast<float>(base_count)
	);
}

auto sandbox::spawn_inverted_mass_pyramid(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	s.spawn(
		"Pyramid Light Base",
		box(
			origin + gse::vec3<gse::length>(0.f, 0.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(5.f)
		)
	);
	s.spawn(
		"Pyramid Mid",
		box(
			origin + gse::vec3<gse::length>(0.f, 1.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(50.f)
		)
	);
	s.spawn(
		"Pyramid Heavy Top",
		box(
			origin + gse::vec3<gse::length>(0.f, 2.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(500.f)
		)
	);
}

auto sandbox::spawn_domino_chain(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float spacing = 0.9f;

	for (int i = 0; i < 12; ++i) {
		const float x = static_cast<float>(i) * spacing;
		const auto pos = origin + gse::vec3<gse::length>(x, (i == 0) ? 1.2f : 1.f, 0.f);
		const auto tilt = (i == 0) ? gse::quat(
			{ 0.f, 0.f, 1.f },
			gse::radians(-0.8f)
		)
								   : gse::quat();
		s.spawn(
			std::format("Domino {}", i + 1),
			box(
				pos,
				gse::vec3<gse::length>(0.3f, 2.f, 1.f),
				gse::kilograms(30.f),
				tilt
			)
		);
	}
}

auto sandbox::spawn_funnel(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float wall_len = 8.f;
	constexpr float wall_height = 4.f;
	constexpr float half_opening = 0.8f;
	constexpr float spread = 4.f;
	const float angle_deg = std::atan2(spread - half_opening, wall_len) * (180.f / 3.14159265f);

	const gse::quat left_rot(gse::axis_y, gse::degrees(angle_deg));
	const gse::quat right_rot(gse::axis_y, gse::degrees(-angle_deg));

	const float half_len = wall_len * 0.5f;
	const float mid_offset = (spread + half_opening) * 0.5f;

	s.spawn(
		"Funnel Left Wall",
		static_box(
			origin + gse::vec3<gse::length>(-mid_offset, wall_height * 0.5f, 0.f),
			gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
			left_rot
		)
	);
	s.spawn(
		"Funnel Right Wall",
		static_box(
			origin + gse::vec3<gse::length>(mid_offset, wall_height * 0.5f, 0.f),
			gse::vec3<gse::length>(wall_len, wall_height, 0.3f),
			right_rot
		)
	);
	s.spawn(
		"Funnel Back Wall",
		static_box(
			origin + gse::vec3<gse::length>(0.f, wall_height * 0.5f, -half_len),
			gse::vec3<gse::length>(spread * 2.f + 1.f, wall_height, 0.3f)
		)
	);

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const auto pos = origin +
				gse::vec3<gse::length>(
				-1.f + static_cast<float>(col) * 1.1f,
					0.5f + static_cast<float>(row) * 1.1f,
				-3.f
				);
			s.spawn(
				std::format("Funnel Box r{}c{}", row, col),
				box(
					pos,
					gse::vec3<gse::length>(gse::meters(1.f)),
					gse::kilograms(40.f)
				)
			);
		}
	}
}

auto sandbox::spawn_slope_friction_test(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const gse::vec3<gse::length> ramp_size(10.f, 0.5f, 4.f);
	const gse::vec3 box_size(gse::meters(1.f));

	const auto resting_offset_for = [&](const gse::quat& tilt) {
		return gse::rotate_vector(tilt, gse::vec3<gse::length>(0.f, ramp_size.y() * 0.5f + box_size.y() * 0.5f, 0.f));
	};

	const gse::quat ramp_tilt(gse::axis_z, gse::degrees(30.f));
	const auto ramp_position = origin + gse::vec3<gse::length>(0.f, 2.f, 0.f);
	s.spawn("Ramp 30deg", static_box(ramp_position, ramp_size, ramp_tilt));
	s.spawn(
		"Ramp Box Should Hold",
		box(
			ramp_position + resting_offset_for(ramp_tilt),
			box_size,
			gse::kilograms(50.f),
			ramp_tilt
		)
	);

	const gse::quat steep_tilt(gse::axis_z, gse::degrees(45.f));
	const auto steep_position = origin + gse::vec3<gse::length>(12.f, 2.f, 0.f);
	s.spawn("Steep Ramp 45deg", static_box(steep_position, ramp_size, steep_tilt));
	s.spawn(
		"Steep Box Should Slide",
		box(
			steep_position + resting_offset_for(steep_tilt),
			box_size,
			gse::kilograms(50.f),
			steep_tilt
		)
	);
}

auto sandbox::spawn_high_speed_impact_target(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const auto pos = origin +
				gse::vec3<gse::length>(
				-1.1f + static_cast<float>(col) * 1.1f,
					0.5f + static_cast<float>(row) * 1.05f,
					0.f
				);
			s.spawn(
				std::format("Impact Wall r{}c{}", row, col),
				box(
					pos,
					gse::vec3<gse::length>(gse::meters(1.f)),
					gse::kilograms(80.f)
				)
			);
		}
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
		const auto anchor_pos = origin + gse::vec3<gse::length>(static_cast<float>(i) * 5.f, 10.f, 0.f);

		const auto anchor_id = s.spawn(
			std::format("Spring {} Anchor", labels[i]),
			static_box(anchor_pos, gse::vec3<gse::length>(0.5f, 0.5f, 0.5f))
		);

		const auto bob_id = s.spawn(
			std::format("Spring {} Bob", labels[i]),
			sphere(
				anchor_pos + gse::vec3<gse::length>(2.f, 0.f, 0.f),
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

	const auto chain_anchor_pos = origin + gse::vec3<gse::length>(18.f, 12.f, 0.f);
	const auto chain_anchor =
		s.spawn(
			"Spring Chain Anchor",
			static_box(chain_anchor_pos, gse::vec3<gse::length>(0.5f, 0.5f, 0.5f))
		);

	auto prev_id = chain_anchor;
	for (int i = 0; i < 5; ++i) {
		const auto link_pos = chain_anchor_pos + gse::vec3<gse::length>(0.f, -static_cast<float>(i + 1) * 2.f, 0.f);
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

auto sandbox::spawn_tumbler(gse::scene& s, const int index, const gse::vec3<gse::position>& center, const gse::vec3f& rotation_axis, const gse::angular_velocity angular_speed, const stress_scene_params& params) -> void {
	const float interior_half = static_cast<float>(params.tumbler_radial_cubes) * 0.5f + 0.5f;
	const float length_half = static_cast<float>(params.tumbler_axial_cubes) * 0.5f;
	constexpr float thickness = 0.3f;
	const float outer_half = interior_half + thickness;
	const float wall_offset = interior_half + thickness * 0.5f;
	const float side_wall_length = (length_half + thickness) * 2.f;

	struct wall_def {
		std::string_view suffix;
		gse::vec3<gse::length> local_offset;
		gse::vec3<gse::length> size;
	};

	const std::array walls = {
		wall_def{
			.suffix = "Bottom",
			.local_offset = gse::vec3<gse::length>(0.f, -wall_offset, 0.f),
			.size = gse::vec3<gse::length>(outer_half * 2.f, thickness, side_wall_length),
		},
		wall_def{
			.suffix = "Top",
			.local_offset = gse::vec3<gse::length>(0.f, wall_offset, 0.f),
			.size = gse::vec3<gse::length>(outer_half * 2.f, thickness, side_wall_length),
		},
		wall_def{
			.suffix = "Left",
			.local_offset = gse::vec3<gse::length>(-wall_offset, 0.f, 0.f),
			.size = gse::vec3<gse::length>(thickness, outer_half * 2.f, side_wall_length),
		},
		wall_def{
			.suffix = "Right",
			.local_offset = gse::vec3<gse::length>(wall_offset, 0.f, 0.f),
			.size = gse::vec3<gse::length>(thickness, outer_half * 2.f, side_wall_length),
		},
		wall_def{
			.suffix = "Front",
			.local_offset = gse::vec3<gse::length>(0.f, 0.f, length_half + thickness * 0.5f),
			.size = gse::vec3<gse::length>(outer_half * 2.f, outer_half * 2.f, thickness),
		},
		wall_def{
			.suffix = "Back",
			.local_offset = gse::vec3<gse::length>(0.f, 0.f, -(length_half + thickness * 0.5f)),
			.size = gse::vec3<gse::length>(outer_half * 2.f, outer_half * 2.f, thickness),
		},
	};

	for (const auto& wall : walls) {
		auto wall_arch = box(center + wall.local_offset, wall.size, gse::kilograms(10000.f));
		wall_arch.motion.body = gse::physics::kinematic_body{};
		wall_arch.spec.material.opacity = 0.2f;
		const auto wall_id = s.spawn(std::format("Tumbler {} Wall {}", index, wall.suffix), std::move(wall_arch));
		s.registry().add_component<tumbler::component>(
			wall_id,
			{
				.center = center,
				.axis = rotation_axis,
				.angular_speed = angular_speed,
				.local_offset = wall.local_offset,
			}
		);
		s.registry().add_component<gse::physics::kinematic_target_component>(
			wall_id,
			{
				.position = center + wall.local_offset,
			}
		);
	}

	const int nx = params.tumbler_radial_cubes;
	const int ny = params.tumbler_radial_cubes;
	const int nz = params.tumbler_axial_cubes;
	constexpr float content_size = 0.5f;
	const float radial_span = interior_half - content_size;
	const float axial_span = length_half - content_size;

	int content_id = 0;
	for (int ix = 0; ix < nx; ++ix) {
		for (int iy = 0; iy < ny; ++iy) {
			for (int iz = 0; iz < nz; ++iz) {
				const float fx = -radial_span + (static_cast<float>(ix) + 0.5f) * (radial_span * 2.f / static_cast<float>(nx));
				const float fy = -radial_span + (static_cast<float>(iy) + 0.5f) * (radial_span * 2.f / static_cast<float>(ny));
				const float fz = -axial_span + (static_cast<float>(iz) + 0.5f) * (axial_span * 2.f / static_cast<float>(nz));
				s.spawn(
					std::format("Tumbler {} Cube {}", index, content_id++),
					box(
						center + gse::vec3<gse::length>(fx, fy, fz),
						gse::vec3<gse::length>(gse::meters(content_size)),
						gse::kilograms(1.f)
					)
				);
			}
		}
	}
}

auto sandbox::spawn_box_grid(gse::scene& s, const gse::vec3<gse::position>& origin, const stress_scene_params& params) -> void {
	const int grid_x = params.box_grid_side;
	const int grid_z = params.box_grid_side;
	const int layers = params.box_grid_layers;
	constexpr float spacing = 1.1f;

	for (int layer = 0; layer < layers; ++layer) {
		for (int ix = 0; ix < grid_x; ++ix) {
			for (int iz = 0; iz < grid_z; ++iz) {
				const auto pos = origin +
					gse::vec3<gse::length>(
						static_cast<float>(ix) * spacing,
						0.5f + static_cast<float>(layer) * 1.05f,
						static_cast<float>(iz) * spacing
					);
				s.spawn(
					std::format("Grid L{}R{}C{}", layer, ix, iz),
					box(
						pos,
						gse::vec3<gse::length>(gse::meters(1.f)),
						gse::kilograms(20.f)
					)
				);
			}
		}
	}
}

auto sandbox::spawn_fixed_joint(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto anchor_pos = origin + gse::vec3<gse::length>(0.f, 5.f, 0.f);
	const auto anchor_id = s.spawn("Fixed Anchor", static_box(anchor_pos, gse::vec3<gse::length>(gse::meters(1.f))));

	const auto hanging_id = s.spawn(
		"Fixed Hanging Box",
		box(
			anchor_pos + gse::vec3<gse::length>(0.f, -1.5f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(20.f)
		)
	);

	s.build("Fixed Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = anchor_id,
			.entity_b = hanging_id,
			.config = gse::physics::fixed_joint{
				.anchor_a = gse::vec3<gse::displacement>(0.f, -0.5f, 0.f),
				.anchor_b = gse::vec3<gse::displacement>(0.f, 0.5f, 0.f),
			},
		});
}

auto sandbox::spawn_distance_pendulum(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto pivot_pos = origin + gse::vec3<gse::length>(0.f, 8.f, 0.f);
	const auto pivot_id = s.spawn("Distance Pivot", static_box(pivot_pos, gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)));

	const auto bob_id = s.spawn(
		"Distance Bob",
		box(
			pivot_pos + gse::vec3<gse::length>(3.f, -3.f, 0.f),
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

auto sandbox::spawn_hinge_door(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto frame_pos = origin + gse::vec3<gse::length>(0.f, 2.f, 0.f);
	const auto frame_id = s.spawn("Hinge Frame", static_box(frame_pos, gse::vec3<gse::length>(0.3f, 4.f, 0.3f)));

	auto door = box(
		frame_pos + gse::vec3<gse::length>(1.5f, 0.f, 0.f),
		gse::vec3<gse::length>(3.f, 3.5f, 0.2f),
		gse::kilograms(40.f)
	);
	if (auto* d = std::get_if<gse::physics::dynamic_body>(&door.motion.body)) {
		d->affected_by_gravity = false;
	}
	const auto door_id = s.spawn("Hinge Door", std::move(door));

	s.build("Hinge Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = frame_id,
			.entity_b = door_id,
			.config = gse::physics::hinge_joint{
				.anchor_a = gse::vec3<gse::displacement>(0.15f, 0.f, 0.f),
				.anchor_b = gse::vec3<gse::displacement>(-1.5f, 0.f, 0.f),
				.axis = { 0.f, 1.f, 0.f },
				.limits = std::pair{ gse::radians(-1.57f), gse::radians(1.57f) },
			},
		});
}

auto sandbox::spawn_slider_elevator(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	const auto rail_pos = origin + gse::vec3<gse::length>(0.f, 4.f, 0.f);
	const auto rail_id = s.spawn("Slider Rail", static_box(rail_pos, gse::vec3<gse::length>(0.3f, 8.f, 0.3f)));

	const auto platform_id = s.spawn(
		"Slider Platform",
		box(
			rail_pos + gse::vec3<gse::length>(0.f, 2.f, 0.f),
			gse::vec3<gse::length>(2.f, 0.3f, 2.f),
			gse::kilograms(30.f)
		)
	);

	s.build("Slider Joint")
		.with<gse::physics::joint_spec>({
			.entity_a = rail_id,
			.entity_b = platform_id,
			.config = gse::physics::slider_joint{
				.axis = { 0.f, 1.f, 0.f },
			},
		});
}

auto sandbox::spawn_pendulum_chain(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr int chain_length = 5;
	constexpr float link_spacing = 1.5f;

	const auto ceiling_pos = origin + gse::vec3<gse::length>(0.f, 12.f, 0.f);
	const auto ceiling_id = s.spawn("Chain Ceiling", static_box(ceiling_pos, gse::vec3<gse::length>(1.f, 0.5f, 1.f)));

	std::vector<gse::id> link_ids;
	link_ids.push_back(ceiling_id);

	for (int i = 0; i < chain_length; ++i) {
		const float y = 12.f - (static_cast<float>(i) + 1.f) * link_spacing;
		const float x_offset = (i == chain_length - 1) ? 2.f : 0.f;
		const auto link_pos = origin + gse::vec3<gse::length>(x_offset, y, 0.f);
		const auto link_id = s.spawn(
			std::format("Chain Link {}", i),
			box(
				link_pos,
				gse::vec3<gse::length>(0.6f, 0.6f, 0.6f),
				gse::kilograms(15.f)
			)
		);
		link_ids.push_back(link_id);
	}

	for (std::size_t i = 0; i + 1 < link_ids.size(); ++i) {
		s.build(std::format("Chain Joint {}", i))
			.with<gse::physics::joint_spec>({
				.entity_a = link_ids[i],
				.entity_b = link_ids[i + 1],
				.config = gse::physics::distance_joint{
					.target = gse::meters(link_spacing),
				},
			});
	}
}

auto sandbox::spawn_sphere_stack(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr int stack_count = 8;
	constexpr float radius = 0.5f;
	constexpr float spacing = radius * 2.f + 0.02f;

	for (int i = 0; i < stack_count; ++i) {
		const auto pos = origin + gse::vec3<gse::length>(
			0.f,
			radius + spacing * static_cast<float>(i),
			0.f
		);
		s.spawn(
			std::format("Sphere Stack {}", i),
			sphere(pos, gse::meters(radius), gse::sphere_lod::mid)
		);
	}
}

auto sandbox::spawn_corner_drop(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	s.spawn(
		"Corner Drop Plate",
		static_box(
			origin + gse::vec3<gse::length>(0.f, 0.25f, 0.f),
			gse::vec3<gse::length>(6.f, 0.5f, 6.f)
		)
	);

	const auto tilt = gse::quat(gse::axis_x, gse::degrees(45.f)) * gse::quat(gse::axis_z, gse::degrees(35.264f));
	s.spawn(
		"Corner Drop Box",
		box(
			origin + gse::vec3<gse::length>(0.f, 8.f, 0.f),
			gse::vec3<gse::length>(gse::meters(1.f)),
			gse::kilograms(20.f),
			tilt
		)
	);
}

auto sandbox::spawn_elevator(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float platform_size = 3.f;
	constexpr float platform_height = 0.3f;
	constexpr float platform_y = 3.f;
	constexpr float oscillation_amplitude = 2.5f;
	constexpr float frequency_hz = 0.4f;
	const float omega_value = 2.f * std::numbers::pi_v<float> * frequency_hz;

	const auto platform_origin = origin + gse::vec3<gse::length>(0.f, platform_y, 0.f);
	auto platform_arch = box(
		platform_origin,
		gse::vec3<gse::length>(platform_size, platform_height, platform_size),
		gse::kilograms(10000.f)
	);
	platform_arch.motion.body = gse::physics::kinematic_body{};

	const auto platform_id = s.spawn("Elevator Platform", std::move(platform_arch));
	s.registry().add_component<piston::component>(
		platform_id,
		{
			.center = platform_origin,
			.amplitude = gse::vec3<gse::length>(0.f, oscillation_amplitude, 0.f),
			.omega = gse::radians_per_second(omega_value),
		}
	);
	s.registry().add_component<gse::physics::kinematic_target_component>(
		platform_id,
		{
			.position = platform_origin,
		}
	);

	constexpr int grid = 3;
	constexpr float box_size = 0.5f;
	constexpr float box_spacing = 0.7f;
	for (int i = 0; i < grid; ++i) {
		for (int j = 0; j < grid; ++j) {
			const auto pos = platform_origin + gse::vec3<gse::length>(
				-box_spacing + static_cast<float>(i) * box_spacing,
				platform_height * 0.5f + box_size * 0.5f + 0.05f,
				-box_spacing + static_cast<float>(j) * box_spacing
			);
			s.spawn(
				std::format("Elevator Box {}{}", i, j),
				box(pos, gse::vec3<gse::length>(gse::meters(box_size)), gse::kilograms(2.f))
			);
		}
	}
}

auto sandbox::spawn_vise(gse::scene& s, const gse::vec3<gse::position>& origin) -> void {
	constexpr float floor_thickness = 1.f;
	constexpr float wall_thickness = 0.3f;
	constexpr float wall_height = 3.f;
	constexpr float wall_depth = 2.f;
	constexpr float wall_center_offset = 1.0f;
	constexpr float wall_amplitude = 0.7f;
	constexpr float frequency_hz = 0.3f;
	const float omega_value = 2.f * std::numbers::pi_v<float> * frequency_hz;

	s.spawn(
		"Vise Floor",
		static_box(
			origin + gse::vec3<gse::length>(0.f, floor_thickness * 0.5f, 0.f),
			gse::vec3<gse::length>(6.f, floor_thickness, wall_depth + 2.f)
		)
	);

	constexpr std::array<std::pair<std::string_view, float>, 2> wall_defs = {
		std::pair{ std::string_view{"Right"}, 1.f },
		std::pair{ std::string_view{"Left"}, -1.f },
	};

	for (const auto& [name, sign] : wall_defs) {
		const auto wall_center = origin + gse::vec3<gse::length>(
			sign * wall_center_offset,
			floor_thickness + wall_height * 0.5f,
			0.f
		);
		auto wall_arch = box(
			wall_center,
			gse::vec3<gse::length>(wall_thickness, wall_height, wall_depth),
			gse::kilograms(10000.f)
		);
		wall_arch.motion.body = gse::physics::kinematic_body{};

		const auto wall_id = s.spawn(std::format("Vise {} Wall", name), std::move(wall_arch));
		s.registry().add_component<piston::component>(
			wall_id,
			{
				.center = wall_center,
				.amplitude = gse::vec3<gse::length>(-sign * wall_amplitude, 0.f, 0.f),
				.omega = gse::radians_per_second(omega_value),
			}
		);
		s.registry().add_component<gse::physics::kinematic_target_component>(
			wall_id,
			{
				.position = wall_center,
			}
		);
	}

	s.spawn(
		"Vise Box",
		box(
			origin + gse::vec3<gse::length>(0.f, floor_thickness + 0.3f, 0.f),
			gse::vec3<gse::length>(gse::meters(0.5f)),
			gse::kilograms(5.f)
		)
	);
}

auto sandbox::spawn_physics_stress(gse::scene& s, const stress_scene_params& params) -> void {
	spawn_inverted_mass_pyramid(s, gse::vec3<gse::position>(-15.f, 0.f, 0.f));
	spawn_domino_chain(s, gse::vec3<gse::position>(-8.f, 0.f, -10.f));
	spawn_funnel(s, gse::vec3<gse::position>(15.f, 0.f, 0.f));
	spawn_slope_friction_test(s, gse::vec3<gse::position>(0.f, 0.f, 15.f));
	spawn_high_speed_impact_target(s, gse::vec3<gse::position>(0.f, 0.f, -20.f));
	spawn_box_grid(s, gse::vec3<gse::position>(20.f, 0.f, -10.f), params);
	spawn_spring_tests(s, gse::vec3<gse::position>(-25.f, 0.f, -20.f));

	const float drum_clearance = static_cast<float>(params.tumbler_radial_cubes) * 0.5f + 2.f;
	spawn_tumbler(s, 0, gse::vec3<gse::position>(-12.f - drum_clearance, 4.f + drum_clearance, 24.f + drum_clearance), gse::axis_z, gse::radians_per_second(0.6f), params);
	spawn_tumbler(s, 1, gse::vec3<gse::position>(12.f + drum_clearance, 4.f + drum_clearance, 24.f + drum_clearance), gse::axis_x, gse::radians_per_second(0.5f), params);

	spawn_sphere_stack(s, gse::vec3<gse::position>(30.f, 0.f, 5.f));
	spawn_corner_drop(s, gse::vec3<gse::position>(30.f, 0.f, 15.f));
	spawn_elevator(s, gse::vec3<gse::position>(-30.f, 0.f, 10.f));
	spawn_vise(s, gse::vec3<gse::position>(0.f, 0.f, 30.f));

	s.spawn("Bouncy Sphere", sphere(gse::vec3<gse::position>(-15.f, 8.f, 0.f), gse::meters(1.f)));
}


auto sandbox::spawn_light_field(gse::scene& s, const light_field_params& params) -> void {
	constexpr float golden_angle = 137.50776f;
	constexpr float golden_ratio_frac = 0.6180339f;
	const auto inverse_reach = 1.f / params.falloff_radius;
	const float reach_ratio = params.intensity / gse::renderer::light_culling::limits.cull_threshold;
	const auto falloff_linear = inverse_reach * 2.f;
	const auto falloff_quadratic = inverse_reach * inverse_reach * std::max(reach_ratio - 3.f, 1.f);

	const bool drifting = params.drift > gse::meters(0.f);
	int spawned = 0;

	for (int c = 0; c < params.clusters; ++c) {
		const auto cluster_index = static_cast<float>(c);
		const auto theta = gse::degrees(golden_angle * cluster_index);
		const auto radial = params.field_radius * std::sqrt((cluster_index + 0.5f) / static_cast<float>(params.clusters));
		const auto height = params.field_height * (golden_ratio_frac * cluster_index - std::floor(golden_ratio_frac * cluster_index));
		const auto center = gse::vec3<gse::position>(radial * gse::sin(theta), height, radial * gse::cos(theta));

		const auto hue = gse::degrees(360.f * (golden_ratio_frac * cluster_index * 3.f - std::floor(golden_ratio_frac * cluster_index * 3.f)));
		const auto color = gse::vec3f(
			0.55f + 0.45f * gse::sin(hue),
			0.55f + 0.45f * gse::sin(hue + gse::degrees(120.f)),
			0.55f + 0.45f * gse::sin(hue + gse::degrees(240.f))
		);

		for (int i = 0; i < params.lights_per_cluster; ++i) {
			const auto local_index = static_cast<float>(i);
			const auto local_theta = gse::degrees(golden_angle * (local_index + cluster_index));
			const auto local_radial = params.cluster_radius * std::sqrt((local_index + 0.5f) / static_cast<float>(params.lights_per_cluster));
			const auto local_height = params.cluster_radius * (2.f * (golden_ratio_frac * local_index - std::floor(golden_ratio_frac * local_index)) - 1.f);
			const auto origin = center + gse::vec3<gse::length>(
				local_radial * gse::sin(local_theta),
				local_height,
				local_radial * gse::cos(local_theta)
			);

			auto light = sphere_light(origin, params.source_radius);
			light.light.color = color;
			light.light.intensity = params.intensity;
			light.light.linear = falloff_linear;
			light.light.quadratic = falloff_quadratic;
			light.spec.material.base_color = color;
			if (drifting) {
				light.motion.body = gse::physics::kinematic_body{};
			}

			const auto light_id = s.spawn(std::format("FieldLight_{}", spawned), std::move(light));
			++spawned;

			if (drifting) {
				s.registry().add_component<piston::component>(
					light_id,
					{
						.center = origin,
						.amplitude = gse::vec3<gse::length>(
							params.drift * gse::sin(local_theta),
							params.drift * 0.35f,
							params.drift * gse::cos(local_theta)
						),
						.omega = params.drift_rate,
						.phase = gse::degrees(golden_angle * static_cast<float>(spawned)),
					}
				);
				s.registry().add_component<gse::physics::kinematic_target_component>(
					light_id,
					{
						.position = origin,
					}
				);
			}
		}
	}

	gse::log::println("light field: {} lights in {} clusters across {:.1f:m} radius", spawned, params.clusters, params.field_radius);
}

auto sandbox::spawn_joint_test(gse::scene& s) -> void {
	spawn_fixed_joint(s, gse::vec3<gse::position>(-20.f, 0.f, 0.f));
	spawn_distance_pendulum(s, gse::vec3<gse::position>(-10.f, 0.f, 0.f));
	spawn_hinge_door(s, gse::vec3<gse::position>(0.f, 0.f, 0.f));
	spawn_slider_elevator(s, gse::vec3<gse::position>(10.f, 0.f, 0.f));
	spawn_pendulum_chain(s, gse::vec3<gse::position>(20.f, 0.f, 0.f));

	s.spawn("Joint Test Sphere", sphere(gse::vec3<gse::position>(0.f, 6.f, -8.f), gse::meters(1.f)));
}

auto sandbox::character_model(const gse::shared_view<gse::asset::data> assets_d) -> gse::resource::handle<gse::skinned_model> {
	return gse::asset::get<gse::skinned_model>(assets_d, "SkinnedModels/x_bot.v3");
}

auto sandbox::character_clips(const gse::shared_view<gse::asset::data> assets_d) -> gse::animation::locomotion_blend {
	return {
		.idle = gse::asset::get<gse::clip_asset>(assets_d, "Clips/idle"),
		.walk = {
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking_backwards"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/left_strafe_walking"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/right_strafe_walking"),
		},
		.run = {
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/running"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/walking_backwards"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/left_strafe"),
			gse::asset::get<gse::clip_asset>(assets_d, "Clips/right_strafe"),
		},
	};
}

auto sandbox::spawn_character(gse::scene& s, const gse::id owner, const gse::resource::handle<gse::skinned_model>& model, const gse::animation::locomotion_blend& clips, const gse::vec3<gse::position>& origin) -> character_rig {
	if (!model.valid()) {
		return {};
	}

	bool clips_ready = clips.idle.valid();
	for (const auto& clip : clips.walk) {
		clips_ready = clips_ready && clip.valid();
	}
	for (const auto& clip : clips.run) {
		clips_ready = clips_ready && clip.valid();
	}
	if (!clips_ready) {
		return {};
	}

	const auto rig = model.resolve();
	const auto bones = rig->bones();
	if (bones.empty()) {
		return {};
	}

	const auto& proxy = rig->proxy();
	const auto proxy_center = origin + proxy.center;
	const auto proxy_mass = gse::kilograms(78.f);

	const auto proxy_id = s.build(std::format("{}.Proxy", owner.tag()))
		.with<gse::physics::transform_component>({
			.position = proxy_center,
		})
		.with<gse::physics::motion_component>({
			.body = gse::physics::dynamic_body{
				.mass = proxy_mass,
				.update_orientation = false,
			},
		})
		.with<gse::physics::collision_component>({
			.shape = gse::physics::capsule_shape{
				.radius = proxy.radius,
				.half_height = proxy.half_height,
			},
		})
		.with<gse::physics::motor_component>({
			.max_force = gse::newtons(12000.f),
			.compliance = 0.02f,
		})
		.identify();

	gse::skeleton_instance_component instance{
		.model = model,
		.proxy = proxy_id,
	};

	const gse::animation::joint_transform root{
		.position = origin,
	};

	std::array<gse::animation::joint_transform, gse::skeleton_instance_component::max_bones> joints{};
	std::array<gse::animation::joint_transform, gse::skeleton_instance_component::max_bones> bodies{};
	const auto bone_count = std::min(bones.size(), joints.size());

	for (std::size_t i = 0; i < bone_count; ++i) {
		const auto& bone = bones[i];

		const auto& parent = bone.parent == gse::skinned_model::no_parent
			? root
			: joints[bone.parent];

		joints[i] = gse::animation::compose(parent, bone.joint_local_offset, bone.joint_local_rotation);

		bodies[i] = gse::animation::compose(joints[i], bone.body_offset, bone.body_rotation);
		const auto& body = bodies[i];

		instance.bones[i] = s.build(std::format("{}.{}", owner.tag(), bone.name))
			.with<gse::physics::transform_component>({
				.position = body.position,
				.orientation = body.orientation,
			})
			.with<gse::physics::motion_component>({
				.body = gse::physics::kinematic_body{},
			})
			.with<gse::physics::collision_component>({
				.shape = bone.shape,
				.resolve_collisions = false,
			})
			.with<gse::physics::kinematic_target_component>({
				.position = body.position,
				.orientation = body.orientation,
			})
			.identify();
	}
	instance.bone_count = static_cast<std::uint32_t>(bone_count);

	for (std::size_t i = 0; i < bone_count; ++i) {
		const auto& bone = bones[i];
		if (bone.parent == gse::skinned_model::no_parent) {
			continue;
		}

		const auto& parent_body = bodies[bone.parent];
		const auto& body = bodies[i];
		const auto pivot = joints[i].position;

		s.build(std::format("{}.{}.Joint", owner.tag(), bone.name))
			.with<gse::physics::joint_spec>({
				.entity_a = instance.bones[bone.parent],
				.entity_b = instance.bones[i],
				.config = gse::physics::ball_joint{
					.anchor_a = gse::inverse_rotate_vector(parent_body.orientation, pivot - parent_body.position),
					.anchor_b = gse::inverse_rotate_vector(body.orientation, pivot - body.position),
				},
			});
	}

	return {
		.character = s.build(owner)
			.with<gse::skeleton_instance_component>(instance)
			.with<gse::clip_player_component>({
				.layers = { gse::clip_layer{ .clip = clips.idle, .weight = 1.f } },
				.layer_count = 1,
			})
			.identify(),
		.proxy = proxy_id,
	};
}

auto sandbox::possess_character(gse::scene& s, const character_rig& rig, const gse::animation::locomotion_blend& clips) -> void {
	s.build(rig.character)
		.with<character_controller::component>({
			.proxy = rig.proxy,
			.clips = clips,
			.possessed = true,
		})
		.with<sidearm::component>({})
		.with<orbit_camera::component>({
			.target = rig.proxy,
			.stepped_views = true,
			.active = true,
			.free_look = true,
		});
}