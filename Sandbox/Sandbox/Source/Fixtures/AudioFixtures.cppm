export module sandbox:audio_fixtures;

import std;
import gse;

import :archetypes;
import :periodic_hop;

export namespace sandbox {
	constexpr std::string_view audio_probe_emitter = "AudioProbeEmitter";
	constexpr std::string_view audio_occlusion_hull = "AudioOcclusionHull";

	enum class audio_occlusion_station : std::uint8_t {
		wall,
		floor,
		sphere,
		capsule,
		hull,
	};

	auto audio_occlusion_emitter_position(
		audio_occlusion_station station
	) -> gse::vec3<gse::position>;

	auto audio_probe_scene_setup(
		gse::scene& s
	) -> void;

	auto audio_occlusion_scene_setup(
		gse::scene& s
	) -> void;

	constexpr std::string_view audio_contact_clip = "Audio/contact_knock";
	constexpr int audio_contact_fallers = 4;

	auto audio_contact_sound() -> gse::audio::contact_sound_component;

	auto audio_contact_faller_name(
		int index
	) -> std::string;

	auto audio_contact_faller_height(
		int index
	) -> gse::length;

	auto spawn_audio_contact_faller(
		gse::scene& s,
		int index
	) -> void;

	auto audio_contact_scene_setup(
		gse::scene& s
	) -> void;

	enum class audio_modal_material : std::uint8_t {
		steel,
		wood,
		glass,
	};

	constexpr std::array audio_modal_materials{
		audio_modal_material::steel,
		audio_modal_material::wood,
		audio_modal_material::glass,
	};

	using audio_modal_material_set = std::array<std::shared_ptr<const gse::acoustic_material>, audio_modal_materials.size()>;

	constexpr int audio_modal_flat_bars = 5;
	constexpr int audio_modal_bars = audio_modal_flat_bars + 1;
	constexpr int audio_modal_field_bars = 96;

	auto audio_modal_material_tag(
		audio_modal_material material
	) -> std::string;

	auto audio_modal_bar_name(
		int index
	) -> std::string;

	auto audio_modal_bar_height(
		int index
	) -> gse::length;

	auto audio_modal_bar_tilt(
		int index
	) -> gse::angle;

	auto audio_modal_bar_position(
		int index
	) -> gse::vec3<gse::position>;

	auto spawn_audio_modal_bar(
		gse::scene& s,
		int index,
		const gse::acoustic_material& material
	) -> void;

	auto spawn_audio_modal_field(
		gse::scene& s,
		const audio_modal_material_set& materials
	) -> void;

	auto spawn_audio_yard_ringers(
		gse::scene& s,
		const audio_modal_material_set& materials
	) -> void;

	auto audio_modal_scene_setup(
		gse::scene& s
	) -> void;

	constexpr std::string_view audio_slide_plate = "AudioSlidePlate";
	constexpr std::string_view audio_roll_ball = "AudioRollBall";

	auto audio_slide_direction() -> gse::vec3f;

	auto audio_roll_ball_radius() -> gse::length;

	auto spawn_audio_slide_plate(
		gse::scene& s,
		const gse::acoustic_material& material
	) -> void;

	auto spawn_audio_roll_ball(
		gse::scene& s,
		const gse::acoustic_material& material
	) -> void;

	auto audio_slide_scene_setup(
		gse::scene& s
	) -> void;

	auto audio_roll_scene_setup(
		gse::scene& s
	) -> void;
}

namespace sandbox {
	struct audio_listener_archetype {
		gse::physics::transform_component transform;
		gse::audio::listener_component listener;
	};

	struct audio_emitter_archetype {
		gse::physics::transform_component transform;
	};

	auto audio_modal_bar_size() -> gse::vec3<gse::length>;

	auto audio_yard_plate_size() -> gse::vec3<gse::length>;

	struct ringing_box {
		std::string name;
		gse::vec3<gse::position> position;
		gse::vec3<gse::length> size;
		gse::quat orientation = gse::quat(1.f, 0.f, 0.f, 0.f);
	};

	auto spawn_ringing_box(
		gse::scene& s,
		const ringing_box& placed,
		const gse::acoustic_material& material
	) -> gse::id;

	auto audio_slide_tilt() -> gse::quat;

	auto audio_slide_ramp_center() -> gse::vec3<gse::position>;

	auto audio_slide_ramp_size() -> gse::vec3<gse::length>;

	struct rolling_ball {
		std::string name;
		gse::vec3<gse::position> position;
		gse::length radius;
	};

	auto spawn_rolling_ball(
		gse::scene& s,
		const rolling_ball& placed,
		const gse::acoustic_material& material
	) -> gse::id;
}

auto sandbox::audio_probe_scene_setup(gse::scene& s) -> void {
	s.spawn("AudioProbeListener", audio_listener_archetype{});
	s.spawn(
		std::string(audio_probe_emitter),
		audio_emitter_archetype{
			.transform = {
				.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(0.f), gse::meters(-2.f)),
			},
		}
	);
}

auto sandbox::audio_occlusion_emitter_position(const audio_occlusion_station station) -> gse::vec3<gse::position> {
	const auto station_spacing = gse::meters(40.f);
	return gse::vec3<gse::position>(
		station_spacing * static_cast<float>(std::to_underlying(station)),
		gse::meters(0.f),
		gse::meters(-8.f)
	);
}

auto sandbox::audio_occlusion_scene_setup(gse::scene& s) -> void {
	const auto toward_listener = gse::vec3<gse::displacement>(gse::meters(1.f), gse::meters(0.f), gse::meters(4.f));
	const auto midway = [&](const audio_occlusion_station station) {
		return audio_occlusion_emitter_position(station) + toward_listener;
	};

	s.spawn("AudioProbeListener", audio_listener_archetype{});
	s.spawn(
		std::string(audio_probe_emitter),
		audio_emitter_archetype{
			.transform = {
				.position = audio_occlusion_emitter_position(audio_occlusion_station::wall),
			},
		}
	);
	s.spawn(
		"AudioOcclusionWall",
		static_collider(
			gse::vec3<gse::position>(gse::meters(-3.f), gse::meters(0.f), gse::meters(-4.f)),
			gse::vec3<gse::length>(gse::meters(6.f), gse::meters(4.f), gse::meters(0.3f))
		)
	);
	s.spawn(
		"AudioOcclusionFloor",
		static_collider(
			midway(audio_occlusion_station::floor) + gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(-0.75f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(20.f), gse::meters(1.f), gse::meters(20.f))
		)
	);
	s.spawn("AudioOcclusionSphere", static_sphere(midway(audio_occlusion_station::sphere), gse::meters(1.5f)));
	s.spawn("AudioOcclusionCapsule", static_capsule(midway(audio_occlusion_station::capsule), gse::meters(1.f), gse::meters(1.5f)));

	const auto prism_half_height = gse::meters(2.f);
	const std::array<gse::vec2<gse::length>, 3> prism_outline{
		gse::vec2<gse::length>(gse::meters(-2.f), gse::meters(-1.2f)),
		gse::vec2<gse::length>(gse::meters(2.f), gse::meters(-1.2f)),
		gse::vec2<gse::length>(gse::meters(0.f), gse::meters(2.4f)),
	};
	std::vector<gse::vec3<gse::length>> prism;
	for (const auto& corner : prism_outline) {
		prism.emplace_back(corner.x(), -prism_half_height, corner.y());
		prism.emplace_back(corner.x(), prism_half_height, corner.y());
	}
	s.spawn(
		std::string(audio_occlusion_hull),
		hull_archetype{
			.transform = {
				.position = midway(audio_occlusion_station::hull),
			},
			.motion = {
				.body = gse::physics::static_body{},
			},
			.collision = {
				.shape = gse::physics::box_shape{
					.size = gse::vec3<gse::length>(gse::meters(4.f), prism_half_height * 2.f, gse::meters(3.6f)),
				},
			},
			.hull = {
				.points = std::move(prism),
			},
		}
	);
}

auto sandbox::audio_contact_sound() -> gse::audio::contact_sound_component {
	return {
		.clip = gse::find_or_generate_id(audio_contact_clip),
	};
}

auto sandbox::audio_contact_faller_name(const int index) -> std::string {
	return std::format("AudioContactFaller_{}", index);
}

auto sandbox::audio_contact_faller_height(const int index) -> gse::length {
	return gse::meters(1.f) * static_cast<float>(index + 1);
}

auto sandbox::spawn_audio_contact_faller(gse::scene& s, const int index) -> void {
	const auto side = gse::meters(1.f);
	const auto spacing = gse::meters(2.f);
	const auto faller = s.spawn(
		audio_contact_faller_name(index),
		box(
			gse::vec3<gse::position>(spacing * static_cast<float>(index), audio_contact_faller_height(index) + side * 0.5f, gse::meters(0.f)),
			gse::vec3<gse::length>(side, side, side),
			gse::kilograms(20.f)
		)
	);
	s.build(faller)
		.with<gse::audio::contact_sound_component>(audio_contact_sound());
}

auto sandbox::audio_contact_scene_setup(gse::scene& s) -> void {
	constexpr int stacked = 3;
	const auto side = gse::meters(1.f);

	s.spawn(
		"AudioContactFloor",
		static_collider(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);
	s.spawn(
		"AudioContactListener",
		audio_listener_archetype{
			.transform = {
				.position = gse::vec3<gse::position>(gse::meters(3.f), gse::meters(1.5f), gse::meters(8.f)),
			},
		}
	);
	for (int level = 0; level < stacked; ++level) {
		const auto resting = s.spawn(
			std::format("AudioContactStack_{}", level),
			box(
				gse::vec3<gse::position>(gse::meters(-4.f), side * (static_cast<float>(level) + 0.5f), gse::meters(0.f)),
				gse::vec3<gse::length>(side, side, side),
				gse::kilograms(20.f)
			)
		);
		s.build(resting)
			.with<gse::audio::contact_sound_component>(audio_contact_sound());
	}
}

auto sandbox::audio_modal_bar_name(const int index) -> std::string {
	return std::format("AudioModalBar_{}", index);
}

auto sandbox::audio_modal_bar_height(const int index) -> gse::length {
	const auto oblique_drop = gse::meters(0.8f);
	return index < audio_modal_flat_bars ? gse::meters(0.2f) * std::pow(2.f, static_cast<float>(index)) : oblique_drop;
}

auto sandbox::audio_modal_bar_tilt(const int index) -> gse::angle {
	return index < audio_modal_flat_bars ? gse::degrees(0.f) : gse::degrees(15.f);
}

auto sandbox::audio_modal_bar_size() -> gse::vec3<gse::length> {
	return gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.02f), gse::meters(0.03f));
}

auto sandbox::audio_modal_bar_position(const int index) -> gse::vec3<gse::position> {
	const auto reach = gse::meters(3.7f);
	const auto between = gse::degrees(30.f);

	const auto size = audio_modal_bar_size();
	const auto tilt = audio_modal_bar_tilt(index);
	const gse::length low_end = (size.x() * gse::sin(tilt) + size.y() * gse::cos(tilt)) * 0.5f;
	const gse::angle bearing = between * static_cast<float>(index - audio_modal_flat_bars / 2);
	return gse::vec3<gse::position>(
		reach * gse::sin(bearing),
		audio_modal_bar_height(index) + low_end,
		-reach * gse::cos(bearing)
	);
}

auto sandbox::audio_modal_material_tag(const audio_modal_material material) -> std::string {
	return std::format("Acoustics/{}", material);
}

auto sandbox::audio_yard_plate_size() -> gse::vec3<gse::length> {
	return gse::vec3<gse::length>(gse::meters(0.4f), gse::meters(0.02f), gse::meters(0.3f));
}

auto sandbox::spawn_ringing_box(gse::scene& s, const ringing_box& placed, const gse::acoustic_material& material) -> gse::id {
	const auto& [name, position, size, orientation] = placed;
	const auto ringer = s.spawn(
		name,
		box(
			position,
			size,
			material.properties().mass_density * (size.x() * size.y() * size.z()),
			orientation
		)
	);
	s.build(ringer)
		.with<gse::audio::acoustic_material_component>({
			.material = material.id(),
		});
	return ringer;
}

auto sandbox::spawn_audio_modal_bar(gse::scene& s, const int index, const gse::acoustic_material& material) -> void {
	spawn_ringing_box(
		s,
		{
			.name = audio_modal_bar_name(index),
			.position = audio_modal_bar_position(index),
			.size = audio_modal_bar_size(),
			.orientation = gse::from_axis_angle(gse::vec3f(0.f, 0.f, 1.f), audio_modal_bar_tilt(index)),
		},
		material
	);
}

auto sandbox::spawn_audio_modal_field(gse::scene& s, const audio_modal_material_set& materials) -> void {
	constexpr int columns = 12;
	constexpr int rows = audio_modal_field_bars / columns;
	const auto spacing = gse::meters(1.f);
	const auto lowest_drop = gse::meters(0.2f);
	const auto drop_step = gse::meters(0.01f);
	const auto hop_period = gse::seconds(0.5f);
	const auto hop_lift = gse::meters_per_second(1.5f);

	const auto size = audio_modal_bar_size();
	for (int index = 0; index < audio_modal_field_bars; ++index) {
		const auto column = static_cast<float>(index % columns) - static_cast<float>(columns - 1) * 0.5f;
		const auto row = static_cast<float>(index / columns) - static_cast<float>(rows - 1) * 0.5f;
		const auto bar = spawn_ringing_box(
			s,
			{
				.name = std::format("AudioModalFieldBar_{}", index),
				.position = gse::vec3<gse::position>(
					spacing * column,
					lowest_drop + drop_step * static_cast<float>(index) + size.y() * 0.5f,
					spacing * row
				),
				.size = size,
			},
			*materials[static_cast<std::size_t>(index) % materials.size()]
		);
		s.build(bar)
			.with<periodic_hop::component>({
				.period = hop_period,
				.kick = gse::vec3<gse::velocity>(gse::meters_per_second(0.f), hop_lift, gse::meters_per_second(0.f)),
				.waited = hop_period * (static_cast<float>(index) / static_cast<float>(audio_modal_field_bars)),
			});
	}
}

auto sandbox::spawn_audio_yard_ringers(gse::scene& s, const audio_modal_material_set& materials) -> void {
	const auto spacing = gse::meters(1.2f);
	const auto drop = gse::meters(1.f);
	const auto bar_row = gse::meters(7.f);
	const auto plate_row = gse::meters(8.2f);
	const auto first = gse::meters(-3.f);
	const auto bar_period = gse::seconds(2.f);
	const auto plate_period = gse::seconds(3.f);
	const auto stagger = gse::seconds(0.5f);
	const auto tilted = gse::from_axis_angle(gse::vec3f(0.f, 0.f, 1.f), gse::degrees(15.f));

	for (std::size_t index = 0; index < materials.size(); ++index) {
		const auto& material = *materials[index];
		const auto along = first + spacing * static_cast<float>(index);
		const auto offset = stagger * static_cast<float>(index);

		const auto bar = spawn_ringing_box(
			s,
			{
				.name = std::format("AudioYardBar_{}", audio_modal_materials[index]),
				.position = gse::vec3<gse::position>(along, drop, bar_row),
				.size = audio_modal_bar_size(),
				.orientation = tilted,
			},
			material
		);
		s.build(bar)
			.with<periodic_hop::component>({
				.period = bar_period + offset,
			});

		const auto plate = spawn_ringing_box(
			s,
			{
				.name = std::format("AudioYardPlate_{}", audio_modal_materials[index]),
				.position = gse::vec3<gse::position>(along, drop, plate_row),
				.size = audio_yard_plate_size(),
				.orientation = tilted,
			},
			material
		);
		s.build(plate)
			.with<periodic_hop::component>({
				.period = plate_period + offset,
			});
	}

	const auto still = gse::meters_per_second(0.f);
	const auto clearance = gse::millimeters(2.f);
	const auto slide_row = gse::meters(10.5f);
	const auto roll_row = gse::meters(11.8f);
	const auto rub_period = gse::seconds(3.f);
	const auto& steel = *materials[std::to_underlying(audio_modal_material::steel)];

	const auto plate_size = audio_yard_plate_size();
	const auto slider = spawn_ringing_box(
		s,
		{
			.name = "AudioYardSlider",
			.position = gse::vec3<gse::position>(gse::meters(2.f), plate_size.y() * 0.5f + clearance, slide_row),
			.size = plate_size,
		},
		steel
	);
	s.build(slider)
		.with<periodic_hop::component>({
			.period = rub_period,
			.kick = gse::vec3<gse::velocity>(gse::meters_per_second(4.f), still, still),
			.reversing = true,
		});

	const auto radius = audio_roll_ball_radius();
	const auto roller = spawn_rolling_ball(
		s,
		{
			.name = "AudioYardRoller",
			.position = gse::vec3<gse::position>(gse::meters(2.f), radius, roll_row),
			.radius = radius,
		},
		steel
	);
	s.build(roller)
		.with<periodic_hop::component>({
			.period = rub_period + stagger,
			.kick = gse::vec3<gse::velocity>(gse::meters_per_second(1.75f), still, still),
			.reversing = true,
		});
}

auto sandbox::audio_modal_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"AudioModalFloor",
		static_collider(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);
	s.spawn(
		"AudioModalListener",
		audio_listener_archetype{
			.transform = {
				.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(1.5f), gse::meters(0.f)),
			},
		}
	);
}

auto sandbox::audio_slide_tilt() -> gse::quat {
	return gse::from_axis_angle(gse::vec3f(0.f, 0.f, 1.f), gse::degrees(-20.f));
}

auto sandbox::audio_slide_direction() -> gse::vec3f {
	return gse::rotate_vector(audio_slide_tilt(), gse::vec3f(1.f, 0.f, 0.f));
}

auto sandbox::audio_slide_ramp_center() -> gse::vec3<gse::position> {
	return gse::vec3<gse::position>(gse::meters(0.f), gse::meters(2.f), gse::meters(0.f));
}

auto sandbox::audio_slide_ramp_size() -> gse::vec3<gse::length> {
	return gse::vec3<gse::length>(gse::meters(12.f), gse::meters(0.5f), gse::meters(3.f));
}

auto sandbox::audio_roll_ball_radius() -> gse::length {
	return gse::meters(0.25f);
}

auto sandbox::spawn_audio_slide_plate(gse::scene& s, const gse::acoustic_material& material) -> void {
	const auto up_the_slope = gse::meters(-4.5f);
	const auto clearance = gse::millimeters(2.f);

	const auto size = audio_yard_plate_size();
	const auto above = (audio_slide_ramp_size().y() + size.y()) * 0.5f + clearance;
	spawn_ringing_box(
		s,
		{
			.name = std::string(audio_slide_plate),
			.position = audio_slide_ramp_center() + gse::rotate_vector(audio_slide_tilt(), gse::vec3<gse::displacement>(up_the_slope, above, gse::meters(0.f))),
			.size = size,
			.orientation = audio_slide_tilt(),
		},
		material
	);
}

auto sandbox::spawn_rolling_ball(gse::scene& s, const rolling_ball& placed, const gse::acoustic_material& material) -> gse::id {
	const auto ball = s.spawn(placed.name, sphere(placed.position, placed.radius));
	s.build(ball)
		.with<gse::audio::acoustic_material_component>({
			.material = material.id(),
		});
	return ball;
}

auto sandbox::spawn_audio_roll_ball(gse::scene& s, const gse::acoustic_material& material) -> void {
	const auto radius = audio_roll_ball_radius();
	spawn_rolling_ball(
		s,
		{
			.name = std::string(audio_roll_ball),
			.position = gse::vec3<gse::position>(gse::meters(0.f), radius, gse::meters(0.f)),
			.radius = radius,
		},
		material
	);
}

auto sandbox::audio_slide_scene_setup(gse::scene& s) -> void {
	s.spawn("AudioSlideRamp", static_collider(audio_slide_ramp_center(), audio_slide_ramp_size(), audio_slide_tilt()));
	s.spawn(
		"AudioSlideListener",
		audio_listener_archetype{
			.transform = {
				.position = audio_slide_ramp_center() + gse::vec3<gse::displacement>(gse::meters(-2.7f), gse::meters(1.5f), gse::meters(3.f)),
			},
		}
	);
}

auto sandbox::audio_roll_scene_setup(gse::scene& s) -> void {
	s.spawn(
		"AudioRollFloor",
		static_collider(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(40.f), gse::meters(1.f), gse::meters(40.f))
		)
	);
	s.spawn(
		"AudioRollListener",
		audio_listener_archetype{
			.transform = {
				.position = gse::vec3<gse::position>(gse::meters(0.f), gse::meters(1.5f), gse::meters(3.f)),
			},
		}
	);
}
