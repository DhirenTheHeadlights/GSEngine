export module sandbox:dev_spawn_system;

import std;
import gse;

import :audio_fixtures;
import :characters;
import :lights;
import :spawn_params;
import :stacks;
import :workloads;

export namespace sandbox {
	struct spawn_stress_request {};

	struct spawn_joints_request {};

	struct spawn_character_request {};

	struct prefetch_character_request {};

	struct spawn_pyramid_request {};

	struct strike_pyramid_request {};

	struct spawn_lights_request {};

	struct prefetch_audio_stress_request {};

	struct spawn_audio_stress_request {};

	struct cycle_audio_stress_request {};

	struct spawn_audio_contact_faller_request {
		int index = 0;
	};

	struct add_contact_sounds_request {};

	struct prefetch_audio_modal_request {};

	struct spawn_audio_modal_bar_request {
		int index = 0;
		audio_modal_material material = audio_modal_material::steel;
	};

	struct spawn_audio_modal_field_request {};

	struct spawn_audio_slide_plate_request {};

	struct spawn_audio_roll_ball_request {};

	struct prefetch_audio_reverb_request {};

	struct play_audio_reverb_knock_request {
		gse::id emitter;
	};
}

export namespace sandbox::dev_spawn {
	using audio_stress_out = gse::channel_write<gse::audio::play_request, gse::audio::play_tone_request, gse::audio::stop_request>;

	struct [[= gse::system_state<"DevSpawn">{}, = gse::settings::category<"Dev Spawn">{}]] data {
		[[
			= gse::settings::describe<"Sizes of the workload built by the physics stress spawn, whether it is triggered "
									  "by F5 or by the physics_stress scenario.">{},
			= gse::settings::scope<gse::settings::scope_kind::project>{}
		]]
		stress_scene_params stress;

		[[
			= gse::settings::describe<"How many characters a spawn request builds.">{},
			= gse::settings::scope<gse::settings::scope_kind::project>{}
		]]
		character_spawn_params characters;

		[[
			= gse::settings::describe<"Size of the pyramid built by a spawn request, whether it came from F9 or the "
									  "pyramid scenario. User-scoped so it can be swept from --engine-setting.">{}
		]]
		pyramid_scene_params pyramid;

		[[
			= gse::settings::describe<"Where and how hard a strike request hits the pyramid. Swept from "
									  "--engine-setting so a capture shot can be reframed without a rebuild.">{}
		]]
		pyramid_strike_params strike;

		[[
			= gse::settings::describe<"Size and brightness of the light field built by a light spawn request.">{}
		]]
		light_field_params lights;

		[[= gse::actions::bind<"Spawn Stress Test", gse::key::f5>{}]]
		[[= gse::actions::hidden{}]]
		gse::actions::handle spawn_stress;

		[[= gse::actions::bind<"Spawn Joints", gse::key::f6>{}]]
		[[= gse::actions::hidden{}]]
		gse::actions::handle spawn_joints;

		[[= gse::actions::bind<"Spawn Character", gse::key::f7>{}]]
		[[= gse::actions::hidden{}]]
		gse::actions::handle spawn_character;

		[[= gse::actions::bind<"Spawn Pyramid", gse::key::f9>{}]]
		[[= gse::actions::hidden{}]]
		gse::actions::handle spawn_pyramid;

		[[= gse::actions::bind<"Ragdoll", gse::key::f8>{}]]
		[[= gse::actions::hidden{}]]
		gse::actions::handle ragdoll;

		gse::id last_character;
		gse::id possessed_character;
		gse::id last_scene;
		std::uint32_t last_generation = 0;
		int pending_characters = 0;
		bool character_warning_logged = false;
		bool in_light_hall = false;
		bool yard_ringers_pending = false;
		std::vector<gse::channel_future<gse::voice_handle>> audio_stress_voices;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& state,
		gse::channel_read<spawn_stress_request, spawn_joints_request, spawn_character_request, prefetch_character_request, spawn_pyramid_request, strike_pyramid_request, spawn_lights_request, prefetch_audio_stress_request, spawn_audio_stress_request, cycle_audio_stress_request, spawn_audio_contact_faller_request, add_contact_sounds_request, prefetch_audio_modal_request, spawn_audio_modal_bar_request, spawn_audio_modal_field_request, spawn_audio_slide_plate_request, spawn_audio_roll_ball_request, prefetch_audio_reverb_request, play_audio_reverb_knock_request> spawn_in,
		gse::channel_write<gse::animation::ragdoll_request> ragdoll_out,
		gse::channel_write<gse::physics::impulse_request> impulse_out,
		gse::channel_write<gse::renderer::atmosphere::sun_request> sun_out,
		audio_stress_out audio_out,
		gse::shared_view<gse::actions::data> actions_d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::async::task<>;
}

namespace sandbox::dev_spawn {
	constexpr std::size_t audio_stress_tones = 96;
	constexpr std::size_t audio_stress_clips = 32;

	auto audio_stress_clip(
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::resource::handle<gse::audio_clip>;

	auto start_audio_stress_voice(
		std::size_t index,
		audio_stress_out audio_out,
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::channel_future<gse::voice_handle>;

	auto cycle_audio_stress(
		data& state,
		audio_stress_out audio_out,
		gse::shared_view<gse::asset::data> assets_d
	) -> void;

	auto loaded_modal_materials(
		gse::shared_view<gse::asset::data> assets_d
	) -> std::optional<audio_modal_material_set>;

	auto audio_reverb_knock(
		gse::shared_view<gse::asset::data> assets_d
	) -> gse::resource::handle<gse::audio_clip>;

	auto warn_of_unloaded_linings(
		gse::shared_view<gse::asset::data> assets_d
	) -> void;
}

auto sandbox::dev_spawn::audio_reverb_knock(const gse::shared_view<gse::asset::data> assets_d) -> gse::resource::handle<gse::audio_clip> {
	return gse::asset::get<gse::audio_clip>(assets_d, std::string(audio_contact_clip));
}

auto sandbox::dev_spawn::warn_of_unloaded_linings(const gse::shared_view<gse::asset::data> assets_d) -> void {
	for (const auto station : { audio_reverb_station::wood, audio_reverb_station::cork }) {
		const auto lining = audio_reverb_lining(station);
		if (!gse::asset::get<gse::acoustic_material>(assets_d, lining).valid()) {
			gse::log::println(
				gse::log::level::warning,
				"dev_spawn: a reverb probe sound was played while the lining {} had not loaded, so walls lined with it absorb as unlined ones",
				lining
			);
		}
	}
}

auto sandbox::dev_spawn::loaded_modal_materials(const gse::shared_view<gse::asset::data> assets_d) -> std::optional<audio_modal_material_set> {
	std::array<gse::resource::handle<gse::acoustic_material>, audio_modal_materials.size()> handles;
	for (const auto [material, handle] : std::views::zip(audio_modal_materials, handles)) {
		handle = gse::asset::get<gse::acoustic_material>(assets_d, audio_modal_material_tag(material));
	}
	if (!std::ranges::all_of(handles, &gse::resource::handle<gse::acoustic_material>::valid)) {
		return std::nullopt;
	}

	audio_modal_material_set loaded;
	for (const auto [handle, slot] : std::views::zip(handles, loaded)) {
		slot = handle.resolve();
	}
	return loaded;
}

auto sandbox::dev_spawn::audio_stress_clip(const gse::shared_view<gse::asset::data> assets_d) -> gse::resource::handle<gse::audio_clip> {
	return gse::asset::get<gse::audio_clip>(assets_d, "Audio/stress_sweep");
}

auto sandbox::dev_spawn::start_audio_stress_voice(const std::size_t index, const audio_stress_out audio_out, const gse::shared_view<gse::asset::data> assets_d) -> gse::channel_future<gse::voice_handle> {
	constexpr float tones_per_octave = 16.f;
	const auto lowest = gse::hertz(110.f);
	const auto gain = gse::percentage<float>::ratio(1.f, static_cast<float>(audio_stress_tones + audio_stress_clips));

	if (index < audio_stress_tones) {
		return audio_out.push<gse::audio::play_tone_request>({
			.pitch = lowest * std::pow(2.f, static_cast<float>(index) / tones_per_octave),
			.gain = gain,
		});
	}
	return audio_out.push<gse::audio::play_request>({
		.clip = audio_stress_clip(assets_d),
		.loop = true,
		.gain = gain,
	});
}

auto sandbox::dev_spawn::cycle_audio_stress(data& state, const audio_stress_out audio_out, const gse::shared_view<gse::asset::data> assets_d) -> void {
	for (std::size_t index = 0; index < state.audio_stress_voices.size(); index += 2) {
		auto& voice = state.audio_stress_voices[index];
		if (const auto* handle = voice.try_get()) {
			audio_out.push<gse::audio::stop_request>({
				.handle = *handle,
			});
		}
		voice = start_audio_stress_voice(index, audio_out, assets_d);
	}
}

auto sandbox::dev_spawn::run(gse::context& ctx, data& state, const gse::channel_read<spawn_stress_request, spawn_joints_request, spawn_character_request, prefetch_character_request, spawn_pyramid_request, strike_pyramid_request, spawn_lights_request, prefetch_audio_stress_request, spawn_audio_stress_request, cycle_audio_stress_request, spawn_audio_contact_faller_request, add_contact_sounds_request, prefetch_audio_modal_request, spawn_audio_modal_bar_request, spawn_audio_modal_field_request, spawn_audio_slide_plate_request, spawn_audio_roll_ball_request, prefetch_audio_reverb_request, play_audio_reverb_knock_request> spawn_in, const gse::channel_write<gse::animation::ragdoll_request> ragdoll_out, const gse::channel_write<gse::physics::impulse_request> impulse_out, const gse::channel_write<gse::renderer::atmosphere::sun_request> sun_out, const audio_stress_out audio_out, const gse::shared_view<gse::actions::data> actions_d, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<gse::asset::data> assets_d) -> gse::async::task<> {
	const auto& cs = gse::actions::current_state(actions_d);
	auto* scene = world_d.active_scene_ptr;
	const gse::scene::mutation_scope _(ctx);

	const bool key_stress = gse::actions::pressed(state.spawn_stress, cs, actions_d);
	const bool key_joints = gse::actions::pressed(state.spawn_joints, cs, actions_d);
	const bool key_character = gse::actions::pressed(state.spawn_character, cs, actions_d);
	const bool req_stress = !spawn_in.of<spawn_stress_request>().empty();
	const bool req_joints = !spawn_in.of<spawn_joints_request>().empty();
	const bool req_character = !spawn_in.of<spawn_character_request>().empty();
	const bool key_pyramid = gse::actions::pressed(state.spawn_pyramid, cs, actions_d);
	const bool req_pyramid = !spawn_in.of<spawn_pyramid_request>().empty();

	const gse::id active_scene_id = scene != nullptr ? scene->id() : gse::id{};
	const bool entered_scene = !(active_scene_id == state.last_scene) || world_d.scene_generation != state.last_generation;
	state.last_scene = active_scene_id;
	state.last_generation = world_d.scene_generation;
	if (entered_scene) {
		state.possessed_character.reset();
		state.in_light_hall = active_scene_id == gse::find_or_generate_id("LightHall");
		state.yard_ringers_pending = active_scene_id == gse::find_or_generate_id("AudioYard");
	}
	const bool entered_pyramid_scene = entered_scene && active_scene_id == gse::find_or_generate_id("Pyramid");
	const bool entered_light_hall_scene = entered_scene && state.in_light_hall;

	if (scene != nullptr && (key_stress || req_stress)) {
		spawn_physics_stress(*scene, state.stress);
	}
	if (scene != nullptr && (key_pyramid || req_pyramid || entered_pyramid_scene)) {
		spawn_pyramid(*scene, state.pyramid);
	}
	if (scene != nullptr && (!spawn_in.of<spawn_lights_request>().empty() || entered_light_hall_scene)) {
		spawn_light_field(*scene, state.lights);
	}
	if (state.in_light_hall) {
		sun_out.push<gse::renderer::atmosphere::sun_request>({
			.elevation = state.lights.sun_elevation,
		});
	}
	if (scene != nullptr && !spawn_in.of<strike_pyramid_request>().empty()) {
		const auto& strike = state.strike;
		if (const float dir_len = magnitude(strike.direction); dir_len > 0.f) {
			const gse::vec3f dir = strike.direction / dir_len;
			for (int row = strike.row_begin; row < strike.row_begin + strike.rows; ++row) {
				const int count = state.pyramid.base_count - row;
				if (count <= 0) {
					continue;
				}
				const int span = std::max(1, static_cast<int>(static_cast<float>(count) * strike.width_fraction));
				for (int col = 0; col < span && col < count; ++col) {
					if (const auto block = gse::try_find(std::format("PyramidBlock_{}_{}", row, col))) {
						impulse_out.push<gse::physics::impulse_request>({
							.target = *block,
							.impulse = gse::vec3<gse::impulse>(
								dir.x() * strike.strength,
								dir.y() * strike.strength,
								dir.z() * strike.strength
							),
						});
					}
				}
			}
		}
	}
	if (scene != nullptr && (key_joints || req_joints)) {
		spawn_joint_test(*scene);
	}
	if (scene != nullptr) {
		for (const auto& req : spawn_in.of<spawn_audio_contact_faller_request>()) {
			spawn_audio_contact_faller(*scene, req.index);
		}
		const auto modal_bars = spawn_in.of<spawn_audio_modal_bar_request>();
		const bool modal_field = !spawn_in.of<spawn_audio_modal_field_request>().empty();
		const bool slide_plate = !spawn_in.of<spawn_audio_slide_plate_request>().empty();
		const bool roll_ball = !spawn_in.of<spawn_audio_roll_ball_request>().empty();
		if (!modal_bars.empty() || modal_field || state.yard_ringers_pending || slide_plate || roll_ball) {
			if (const auto materials = loaded_modal_materials(assets_d)) {
				const auto& steel = *(*materials)[std::to_underlying(audio_modal_material::steel)];
				for (const auto& req : modal_bars) {
					spawn_audio_modal_bar(*scene, req.index, *(*materials)[std::to_underlying(req.material)]);
				}
				if (modal_field) {
					spawn_audio_modal_field(*scene, *materials);
				}
				if (slide_plate) {
					spawn_audio_slide_plate(*scene, steel);
				}
				if (roll_ball) {
					spawn_audio_roll_ball(*scene, steel);
				}
				if (state.yard_ringers_pending) {
					spawn_audio_yard_ringers(*scene, *materials);
					state.yard_ringers_pending = false;
				}
			}
			else if (!modal_bars.empty() || modal_field || slide_plate || roll_ball) {
				gse::log::println(
					gse::log::level::warning,
					"dev_spawn: {} modal bars, {} modal fields, {} slide plates and {} roll balls were not spawned because the acoustic materials are not loaded yet",
					modal_bars.size(),
					modal_field ? 1 : 0,
					slide_plate ? 1 : 0,
					roll_ball ? 1 : 0
				);
			}
		}
		if (!spawn_in.of<add_contact_sounds_request>().empty()) {
			const std::vector<gse::id> entities(std::from_range, scene->entities());
			for (const gse::id entity : entities) {
				scene->build(entity)
					.with<gse::audio::contact_sound_component>(audio_contact_sound());
			}
		}
	}
	if (!spawn_in.of<prefetch_audio_stress_request>().empty()) {
		audio_stress_clip(assets_d);
	}
	if (!spawn_in.of<prefetch_audio_modal_request>().empty()) {
		loaded_modal_materials(assets_d);
	}
	if (!spawn_in.of<prefetch_audio_reverb_request>().empty()) {
		audio_reverb_knock(assets_d);
		for (const auto station : { audio_reverb_station::wood, audio_reverb_station::cork }) {
			gse::asset::get<gse::acoustic_material>(assets_d, audio_reverb_lining(station));
		}
	}
	const auto reverb_knocks = spawn_in.of<play_audio_reverb_knock_request>();
	if (!reverb_knocks.empty()) {
		warn_of_unloaded_linings(assets_d);
	}
	for (const auto& req : reverb_knocks) {
		audio_out.push<gse::audio::play_request>({
			.clip = audio_reverb_knock(assets_d),
			.gain = gse::percentage<float>::fraction(0.25f),
			.emitter = req.emitter,
		});
	}
	if (!spawn_in.of<spawn_audio_stress_request>().empty()) {
		for (std::size_t index = 0; index < audio_stress_tones + audio_stress_clips; ++index) {
			state.audio_stress_voices.push_back(start_audio_stress_voice(index, audio_out, assets_d));
		}
	}
	if (!spawn_in.of<cycle_audio_stress_request>().empty()) {
		cycle_audio_stress(state, audio_out, assets_d);
	}
	if (!spawn_in.of<prefetch_character_request>().empty()) {
		character_model(assets_d);
		character_clips(assets_d);
	}
	if (key_character || req_character) {
		state.pending_characters = state.characters.count;
	}
	if (scene != nullptr && state.pending_characters > 0) {
		constexpr float character_spacing = 2.f;
		const auto rig = character_model(assets_d);
		const auto clips = character_clips(assets_d);

		character_rig spawned;
		for (int i = 0; i < state.pending_characters; ++i) {
			spawned = spawn_character(
				*scene,
				gse::generate_id(std::format("Character {}", i)),
				rig,
				clips,
				gse::vec3<gse::position>(state.characters.origin_x + gse::meters(static_cast<float>(i) * character_spacing), gse::meters(0.f), state.characters.origin_z)
			);
			if (!spawned.character.exists()) {
				break;
			}
			state.last_character = spawned.character;
			if (!state.possessed_character.exists()) {
				state.possessed_character = spawned.character;
				possess_character(*scene, spawned, clips);
			}
		}

		if (spawned.character.exists()) {
			state.pending_characters = 0;
		}
		else if (!state.character_warning_logged) {
			state.character_warning_logged = true;
			gse::log::println(
				gse::log::level::warning,
				"dev_spawn: character spawn produced nothing, so its skinned model or clips are not resolved yet; retrying each tick"
			);
		}
	}
	if (state.last_character.exists() && gse::actions::pressed(state.ragdoll, cs, actions_d)) {
		ragdoll_out.push<gse::animation::ragdoll_request>({
			.character = state.last_character,
		});
	}

	return {};
}