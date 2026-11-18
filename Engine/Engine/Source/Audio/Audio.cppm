export module gse.audio;

import gse.assets;
import gse.concurrency;
import gse.config;
import gse.containers;
import gse.core;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.physics;
import std;

export import :contact;
export import :material;
export import :mixer;
export import :modal;
export import :spatial;
export import :wav;

export namespace gse {
	class audio_clip : public identifiable {
	public:
		struct [[
			= asset_format::baked_ext<".gaud">{},
			= asset_format::baked_dir<"Audio">{},
			= asset_format::source_dir<"Audio">{},
			= asset_format::source_exts<".wav", ".mp3", ".ogg", ".flac">{},
			= asset_format::magic<0x47415544>{},
			= asset_format::version<2>{}
		]] baked {
			raw_blob_owned<std::byte> bytes;
		};

		explicit audio_clip(
			const std::filesystem::path& filepath
		) : identifiable(config::asset_tag(filepath)), m_path(filepath.native_encoded_string()) {
		}

		auto load(
			asset::load_ctx& ctx
		) -> async::task<asset_result>;

		auto samples() const -> std::span<const float>;

	private:
		std::string m_path;
		std::vector<float> m_samples;
	};

	auto bake(
		const std::filesystem::path& src,
		audio_clip::baked& out
	) -> bool;

	struct voice_handle {
		std::uint32_t index = 0;
		std::uint32_t generation = 0;
	};
}

export namespace gse::audio {
	using asset_types = type_pack<audio_clip, acoustic_material>;

	struct output_device;

	struct device_deleter {
		auto operator()(
			output_device* device
		) const -> void;
	};

	using device_ptr = std::unique_ptr<output_device, device_deleter>;

	struct play_request {
		using result_type = voice_handle;
		resource::handle<audio_clip> clip;
		bool loop = false;
		percentage<float> gain = percentage<float>::one();
		id emitter;
		channel_promise<voice_handle> promise;
	};

	struct play_tone_request {
		using result_type = voice_handle;
		frequency pitch;
		percentage<float> gain = percentage<float>::one();
		id emitter;
		channel_promise<voice_handle> promise;
	};

	struct stop_request {
		voice_handle handle;
	};

	struct offline_sink_request {
		std::filesystem::path path;
	};

	struct voice_slot {
		std::uint32_t generation = 0;
		bool active = false;
		std::shared_ptr<const audio_clip> clip;
		std::shared_ptr<const acoustic_material> material;
		std::uint64_t rung_out_step = 0;
		bool placed = false;
		id emitter;
		id rubbing;
		std::array<id, 2> unheard;
		vec3<current_position> emitter_position;
		float occluded = 0.f;
	};

	struct offline_sink {
		std::filesystem::path path;
		std::vector<float> samples;
	};

	struct heard_contact {
		physics::contact_event contact;
		impulse delivered;
	};

	struct [[
		= system_state<"Audio">{},
		= settings::category<"Audio">{},
		= requires_setting<^^launch::data::audio, launch::audio_output::device, launch::audio_output::offline>{},
		= requires_setting<^^launch::data::simulate_world, true>{},
		= deferred_system{}
	]]
	data {
		[[
			= settings::describe<
				"What the output is heard through. Speakers pans each positioned sound between the "
				"two channels. Headphones gives each ear its own arrival time and head shadow from "
				"a spherical-head model, which only sounds right when each channel reaches one ear."
			>{},
			= settings::hot_reloadable
		]]
		listening_mode listening = listening_mode::speakers;

		[[
			= settings::describe<
				"Log every contact event the audio system receives: its step, both bodies, impulse, "
				"relative speed and point, whether it started a voice or why it did not, and the "
				"output frame the voice starts on. A body that starts ringing also logs its "
				"material, box size and every baked mode, and every fixed step logs each audible "
				"sliding or rolling contact on a body with a material: its slip and roll speed, load, kind and "
				"level. The contact, modal, slide and roll probe scenarios pin it on; a pile "
				"collapsing logs hundreds of lines a step, so it is off otherwise."
			>{}
		]]
		bool trace_contacts = false;

		[[
			= settings::describe<
				"Give sliding and rolling contacts on a body with an acoustic material a sustained "
				"sound. Off leaves impacts ringing alone, which is what the modal probe scenarios "
				"pin so that a bar skidding after it lands does not scrape over the decay they measure."
			>{},
			= settings::hot_reloadable
		]]
		bool rub_sounds = true;

		launch::audio_output mode = launch::audio_output::device;
		std::unique_ptr<mixer> output;
		device_ptr device;
		offline_sink sink;
		std::vector<voice_slot> slots;
		std::vector<std::uint32_t> free_slots;
		std::vector<physics::ray_target> ray_targets;
		std::vector<physics::ray_target> targets_in_reach;
		std::vector<heard_contact> loudest_contacts;
		std::uint32_t frames_per_step = 0;
		std::uint32_t sounding_voices = 0;
		std::uint32_t peak_voices = 0;
		std::uint64_t voices_started = 0;
		std::uint32_t peak_modal_voices = 0;
		std::uint64_t frames_at_modal_limit = 0;
		std::uint64_t frames_run = 0;
		std::uint64_t rubbed_step = 0;
		std::uint32_t peak_rub_voices = 0;
	};

	[[= system_init{}]]
	auto init(
		shared_view<launch::data> launch_s,
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys_s,
		shared_view<asset::data> assets_s,
		channel_read<play_request, play_tone_request, stop_request, offline_sink_request, physics::contact_event> requests_in,
		read<listener_component> listeners,
		write<contact_sound_component> contact_sounds,
		write<acoustic_material_component> materials,
		read<physics::transform_component> transforms,
		read<physics::collision_component> colliders,
		read<physics::motion_component> motions
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

namespace gse::audio {
	auto open_device(
		mixer& output
	) -> device_ptr;

	auto stop_and_report(
		output_device& device,
		std::uint32_t peak_voices
	) -> void;

	auto silent(
		const data& d
	) -> bool;

	struct scene_listener {
		id owner;
		id body;
		physics::transform_component transform;
	};

	struct located_emitter {
		id owner;
		std::array<id, 2> unheard;
		vec3<current_position> position;
		float occluded = 0.f;
		placement at;
	};

	auto blocked_share(
		data& d,
		const scene_listener& listener,
		const std::array<id, 2>& unheard,
		const vec3<current_position>& emitter_position
	) -> float;

	auto locate(
		data& d,
		id emitter,
		const std::optional<scene_listener>& listener,
		const read<physics::transform_component>& transforms
	) -> std::optional<located_emitter>;

	auto locate(
		data& d,
		const physics::contact_event& contact,
		const scene_listener& listener
	) -> located_emitter;

	auto contact_voices(
		const data& d
	) -> std::uint32_t;

	struct contact_scene {
		const std::optional<scene_listener>& listener;
		std::uint64_t physics_step = 0;
		shared_view<asset::data> assets;
		const write<contact_sound_component>& contact_sounds;
		const write<acoustic_material_component>& materials;
		const read<physics::transform_component>& transforms;
		const read<physics::collision_component>& colliders;
		const read<physics::motion_component>& motions;
	};

	auto delivered_impulse(
		const physics::contact_event& contact,
		const read<physics::motion_component>& motions
	) -> impulse;

	auto sounds(
		const physics::contact_event& contact,
		const write<contact_sound_component>& contact_sounds,
		const write<acoustic_material_component>& materials,
		const read<physics::motion_component>& motions
	) -> bool;

	auto modal_voices(
		const data& d
	) -> std::uint32_t;

	auto ring_body(
		data& d,
		const heard_contact& heard,
		id body,
		const contact_scene& scene,
		std::uint32_t& started
	) -> std::string_view;

	auto trace_modes(
		id body,
		const acoustic_material& material,
		const acoustic_box& box
	) -> void;

	auto start_contact_voices(
		data& d,
		std::span<const physics::contact_event> contacts,
		const contact_scene& scene
	) -> void;

	auto rub_voices(
		const data& d
	) -> std::uint32_t;

	struct rubbed_body {
		const physics::moving_contact& contact;
		id body;
		id other;
		const physics::transform_component& transform;
		rub heard;
		std::uint32_t ramp_frames = 0;
	};

	auto rub_body(
		data& d,
		const rubbed_body& rubbed,
		const contact_scene& scene,
		std::uint32_t& started
	) -> std::string_view;

	auto rub_contacts(
		data& d,
		std::span<const physics::moving_contact> moving,
		const contact_scene& scene
	) -> void;

	auto rub_fade_frames() -> std::uint32_t;

	auto fade_rubs(
		data& d,
		std::uint64_t held_before
	) -> void;

	auto stop_slot(
		data& d,
		std::uint32_t index
	) -> bool;

	auto stop_rung_out_voices(
		data& d,
		std::uint64_t physics_step
	) -> void;

	auto start_voice(
		data& d,
		voice_command command,
		std::shared_ptr<const audio_clip> clip,
		const std::optional<located_emitter>& source
	) -> voice_handle;

	auto stop_orphaned_voices(
		data& d,
		const read<physics::transform_component>& transforms
	) -> void;

	auto move_placed_voices(
		data& d,
		const scene_listener& listener,
		const read<physics::transform_component>& transforms
	) -> void;

	auto valid_voice(
		const data& d,
		voice_handle handle
	) -> bool;

	auto render_fixed_steps(
		data& d
	) -> void;

	auto write_sink(
		const data& d
	) -> void;
}