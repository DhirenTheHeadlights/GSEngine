export module gse.audio;

import std;

import gse.core;
import gse.concurrency;
import gse.config;
import gse.assets;
import gse.containers;
import gse.ecs;
import gse.math;

export namespace gse {
	class audio_clip : public identifiable {
	public:
		struct [[
			= asset_format::baked_ext<".gaud">{},
			= asset_format::baked_dir<"Audio">{},
			= asset_format::source_dir<"Audio">{},
			= asset_format::source_exts<".wav", ".mp3", ".ogg", ".flac">{},
			= asset_format::magic<0x47415544>{},
			= asset_format::version<1>{}
		]] baked {
			raw_blob_owned<std::byte> bytes;
		};

		explicit audio_clip(
			const std::filesystem::path& filepath
		) : identifiable(filepath, config::baked_resource_path()), m_path(filepath.native_encoded_string()) {
		}

		auto load(
			asset::load_ctx& ctx
		) -> async::task<>;

		auto unload() -> void;

		auto data() const -> const std::vector<std::byte>&;

		auto sample_rate() const -> std::uint32_t;

		auto channels() const -> std::uint32_t;

		auto frame_count() const -> std::uint64_t;

		auto duration() const -> time_t<float, seconds>;

	private:
		std::string m_path;
		std::vector<std::byte> m_bytes;
		std::uint32_t m_sample_rate = 0;
		std::uint32_t m_channels = 0;
		std::uint64_t m_frame_count = 0;
		time_t<float, seconds> m_duration;
	};

	auto bake(
		const std::filesystem::path& src,
		audio_clip::baked& out
	) -> bool;
}

export namespace gse {
	struct voice_handle {
		std::uint32_t index = 0;
		std::uint32_t generation = 0;
	};
}

export namespace gse::audio {
	using asset_types = type_pack<audio_clip>;

	struct voice_slot;
	struct audio_engine;

	struct play_request {
		using result_type = voice_handle;
		const audio_clip* clip = nullptr;
		bool loop = false;
		channel_promise<voice_handle> promise;
	};

	struct stop_request {
		voice_handle handle;
	};

	struct pause_request {
		voice_handle handle;
	};

	struct resume_request {
		voice_handle handle;
	};

	struct set_volume_request {
		voice_handle handle;
		percentage<float> vol;
	};

	struct set_master_volume_request {
		percentage<float> vol;
	};

	struct [[= gse::system_state<"Audio">{}, = gse::deferred_system{}]] data {
		audio_engine* engine = nullptr;
		bool engine_initialized = false;
		percentage<float> master_vol = percentage<float>::one();
		std::vector<voice_slot*> voices;
		std::vector<std::uint32_t> free_list;
	};

	[[= gse::system_init{}]]
	auto init(
		data& d
	) -> async::task<>;

	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		data& d
	) -> async::task<>;

	[[= gse::system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;

	auto allocate_voice(
		data& d,
		const audio_clip& clip,
		bool looping
	) -> voice_handle;

	auto release_voice(
		data& d,
		voice_handle handle
	) -> void;

	auto valid_voice(
		const data& d,
		voice_handle handle
	) -> bool;
}
