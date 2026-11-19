export module gse.audio:reverb_network;

import std;

import gse.math;

export namespace gse::audio {
	constexpr std::uint32_t reverb_reflection_count = 6;
	constexpr std::uint32_t reverb_line_count = 8;

	struct reflection {
		time delay;
		float left = 0.f;
		float right = 0.f;
	};

	struct reverb_setting {
		time spacing;
		time decay;
		float send = 0.f;
		std::array<reflection, reverb_reflection_count> early{};
	};

	class reverb_network {
	public:
		reverb_network();

		auto tune(
			const reverb_setting& setting,
			frequency rate,
			std::uint32_t ramp_frames
		) -> void;

		auto process(
			std::span<const float> feed,
			std::span<float> out
		) -> void;

	private:
		struct tap {
			float delay_frames = 0.f;
			float left = 0.f;
			float right = 0.f;
		};

		struct levels {
			float send = 0.f;
			std::array<float, reverb_line_count> feedback{};
			std::array<tap, reverb_reflection_count> early{};
		};

		static constexpr std::uint32_t line_frames = 1u << 13;
		static constexpr std::uint32_t early_frames = 1u << 14;
		static constexpr std::uint32_t output_channels = 2;

		static auto ramp_step(
			const levels& from,
			const levels& to,
			std::uint32_t frames
		) -> levels;

		static auto advance(
			levels& now,
			const levels& step
		) -> void;

		auto feedback_for(
			float decay_frames
		) const -> std::array<float, reverb_line_count>;

		auto send_for(
			const std::array<float, reverb_line_count>& feedback
		) const -> float;

		auto wake() -> void;

		auto rest() -> void;

		auto reflected(
			const tap& early
		) const -> float;

		std::vector<float> m_lines;
		std::vector<float> m_early;
		std::array<std::uint32_t, reverb_line_count> m_lengths{};
		std::array<std::uint32_t, reverb_line_count> m_cursors{};
		std::uint32_t m_early_head = 0;
		levels m_now;
		levels m_target;
		levels m_step;
		std::uint32_t m_ramp_left = 0;
		float m_spacing_frames = 0.f;
		float m_decay_frames = 0.f;
		float m_send = 0.f;
		std::uint32_t m_quiet_frames = 0;
		bool m_awake = false;
	};
}

namespace gse::audio {
	auto next_prime(
		std::uint32_t from
	) -> std::uint32_t;
}
