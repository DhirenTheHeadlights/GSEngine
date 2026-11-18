export module gse.audio:mixer;

import std;

import gse.concurrency;
import gse.core;
import gse.math;

export namespace gse::audio {
	constexpr std::uint32_t voice_capacity = 256;
	constexpr std::uint32_t output_channels = 2;
	constexpr std::uint32_t output_frame_rate = 48000;
	constexpr std::uint32_t resonance_capacity = 24;

	auto sample_rate() -> frequency;

	struct ear {
		time delay;
		percentage<float> gain = percentage<float>::one();
		float shadow = 1.f;
	};

	struct placement {
		ear left;
		ear right;
		frequency shadow_corner;
		frequency absorption_cutoff;
	};

	struct start_clip {
		std::uint32_t slot = 0;
		std::span<const float> samples;
		bool loop = false;
		percentage<float> gain = percentage<float>::one();
		std::optional<placement> at;
	};

	struct start_tone {
		std::uint32_t slot = 0;
		frequency pitch;
		percentage<float> gain = percentage<float>::one();
		std::optional<placement> at;
	};

	struct stop_voice {
		std::uint32_t slot = 0;
	};

	struct move_voice {
		std::uint32_t slot = 0;
		placement at;
		std::uint32_t ramp_frames = 0;
	};

	struct resonance {
		frequency pitch;
		inverse_time decay;
	};

	using strike_drive = std::array<float, resonance_capacity>;

	struct start_ringing {
		std::uint32_t slot = 0;
		std::span<const resonance> rings;
		strike_drive drive{};
		placement at;
	};

	struct strike_voice {
		std::uint32_t slot = 0;
		strike_drive drive{};
	};

	struct start_rubbing {
		std::uint32_t slot = 0;
		std::span<const resonance> rings;
		strike_drive drive{};
		inverse_time least_decay;
		placement at;
	};

	struct rub_voice {
		std::uint32_t slot = 0;
		float level = 0.f;
		frequency cutoff;
		frequency wobble;
		float wobble_depth = 0.f;
		std::uint32_t ramp_frames = 0;
	};

	struct fade_rub {
		std::uint32_t slot = 0;
		std::uint32_t ramp_frames = 0;
	};

	using voice_command = std::variant<start_clip, start_tone, stop_voice, move_voice, start_ringing, strike_voice, start_rubbing, rub_voice, fade_rub>;

	class mixer {
	public:
		mixer();

		auto submit(
			const voice_command& command
		) -> bool;

		auto take_finished(
			std::uint32_t& slot
		) -> bool;

		auto render(
			std::span<float> out
		) -> void;

	private:
		struct clip_cursor {
			std::span<const float> samples;
			std::size_t next = 0;
			bool loop = false;
		};

		struct oscillator {
			float coefficient = 0.f;
			float cosine = 0.f;
			float sine = 0.f;
		};

		struct ear_arrival {
			double delay_frames = 0.0;
			float gain = 0.f;
			float shadow = 1.f;
		};

		struct arrival {
			ear_arrival left;
			ear_arrival right;
			float damping = 0.f;
		};

		struct ear_filter {
			float low_passed = 0.f;
			float shadow_input = 0.f;
			float shadow_high = 0.f;
		};

		struct propagation {
			arrival now;
			arrival target;
			arrival step;
			std::uint32_t ramp_frames = 0;
			std::uint32_t head = 0;
			std::uint32_t written = 0;
			std::uint32_t silent_frames = 0;
			float shadow_feed = 0.f;
			ear_filter left;
			ear_filter right;
		};

		struct ringing {};

		struct resonator {
			float turn_real = 0.f;
			float turn_imag = 0.f;
			float real = 0.f;
			float imag = 0.f;
		};

		struct rubbing {
			std::uint32_t noise = 0;
			float level = 0.f;
			float target = 0.f;
			float level_step = 0.f;
			std::uint32_t ramp_left = 0;
			float feed = 0.f;
			float makeup = 0.f;
			float low_passed = 0.f;
			float wobble_turns = 0.f;
			float wobble_step = 0.f;
			float wobble_depth = 0.f;
		};

		struct bank {
			std::array<resonator, resonance_capacity> modes{};
			strike_drive drive{};
			std::uint32_t count = 0;
			std::uint32_t pulse_left = 0;
			std::optional<rubbing> rub;
		};

		struct voice {
			std::variant<std::monostate, clip_cursor, oscillator, ringing> source;
			float gain = 0.f;
			bool placed = false;
		};

		static constexpr std::size_t command_capacity = 1024;
		static constexpr std::size_t finished_capacity = 512;
		static constexpr std::uint32_t delay_line_frames = 1u << 15;
		static constexpr std::size_t chunk_frames = 256;

		static auto arrival_of(
			const ear& at
		) -> ear_arrival;

		static auto arrival_of(
			const placement& at
		) -> arrival;

		static auto shadow_feed_of(
			const placement& at
		) -> float;

		static auto ramp_step(
			const ear_arrival& from,
			const ear_arrival& to,
			std::uint32_t frames
		) -> ear_arrival;

		static auto advance(
			ear_arrival& now,
			const ear_arrival& step
		) -> void;

		static auto heard(
			std::span<const float> line,
			const propagation& path,
			const ear_arrival& at,
			ear_filter& filter
		) -> float;

		auto place_voice(
			std::uint32_t slot,
			const std::optional<placement>& at
		) -> bool;

		auto apply(
			const move_voice& command
		) -> void;

		auto mix_placed(
			std::uint32_t slot,
			std::span<float> out
		) -> bool;

		static auto advance(
			propagation& path
		) -> void;

		static auto delayed(
			std::span<const float> line,
			const propagation& path,
			double delay_frames
		) -> float;

		static auto tap(
			std::span<const float> line,
			const propagation& path,
			std::uint32_t age
		) -> float;

		static auto fill(
			clip_cursor& cursor,
			float gain,
			std::span<float> dry
		) -> std::size_t;

		static auto fill(
			oscillator& tone,
			float gain,
			std::span<float> dry
		) -> std::size_t;

		static auto fill(
			bank& struck,
			std::span<float> dry
		) -> std::size_t;

		static auto fill(
			bank& rubbed,
			rubbing& rub,
			std::span<float> dry
		) -> void;

		static auto ring(
			std::span<resonator> modes,
			const strike_drive& drive,
			float push
		) -> float;

		static auto settle(
			std::span<resonator> modes
		) -> void;

		static auto tune(
			bank& struck,
			std::span<const resonance> rings,
			inverse_time least_decay
		) -> void;

		static auto next(
			oscillator& tone
		) -> float;

		auto apply(
			const start_ringing& command
		) -> void;

		auto apply(
			const start_rubbing& command
		) -> void;

		static auto ramp(
			rubbing& rub,
			float target,
			std::uint32_t frames
		) -> void;

		auto apply(
			const rub_voice& command
		) -> void;

		auto apply(
			const fade_rub& command
		) -> void;

		auto apply(
			const strike_voice& command
		) -> void;

		auto apply(
			const start_clip& command
		) -> void;

		auto apply(
			const start_tone& command
		) -> void;

		auto apply(
			const stop_voice& command
		) -> void;

		auto finish(
			std::uint32_t slot
		) -> void;

		static auto mix(
			clip_cursor& cursor,
			float gain,
			std::span<float> out
		) -> bool;

		static auto mix(
			oscillator& tone,
			float gain,
			std::span<float> out
		) -> bool;

		std::array<voice, voice_capacity> m_voices{};
		std::vector<propagation> m_paths;
		std::vector<bank> m_banks;
		std::vector<float> m_delay_lines;
		spsc_ring_buffer<voice_command, command_capacity> m_commands;
		spsc_ring_buffer<std::uint32_t, finished_capacity> m_finished;
	};
}
