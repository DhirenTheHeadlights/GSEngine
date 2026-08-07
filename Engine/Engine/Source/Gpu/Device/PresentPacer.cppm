export module gse.gpu:present_pacer;

import std;

import gse.gpu_backend;
import gse.math;

export namespace gse::gpu {
	constexpr std::uint32_t stale_observe_limit = 900;

	class present_pacer {
	public:
		auto observe(
			std::span<const past_present_timing> samples,
			time_t<std::uint64_t> refresh_interval
		) -> void;

		[[nodiscard]] auto has_feedback() const -> bool;

		[[nodiscard]] auto frame_delta() const -> time;

		[[nodiscard]] auto samples_seen() const -> std::uint64_t;

		[[nodiscard]] auto samples_used() const -> std::uint64_t;

	private:
		std::uint64_t m_last_present_id = 0;
		time_t<std::uint64_t> m_last_first_pixel_out{};
		time m_smoothed{};
		time m_interval{};
		time_t<std::uint64_t> m_refresh_interval{};
		std::uint64_t m_samples_seen = 0;
		std::uint64_t m_samples_used = 0;
		std::uint32_t m_multiple = 0;
		std::uint32_t m_observes_since_use = 0;
		bool m_has_baseline = false;
	};
}

auto gse::gpu::present_pacer::observe(const std::span<const past_present_timing> samples, const time_t<std::uint64_t> refresh_interval) -> void {
	m_refresh_interval = refresh_interval;

	std::vector<std::pair<std::uint64_t, time_t<std::uint64_t>>> points;
	points.reserve(samples.size());
	for (const auto& sample : samples) {
		++m_samples_seen;
		if (sample.first_pixel_out > time_t<std::uint64_t>{}) {
			points.emplace_back(sample.present_id, sample.first_pixel_out);
		}
	}
	std::ranges::sort(points);

	for (const auto& [id, first_pixel_out] : points) {
		if (m_has_baseline && id > m_last_present_id && first_pixel_out > m_last_first_pixel_out) {
			m_observes_since_use = 0;
			const auto id_delta = id - m_last_present_id;
			const auto span = first_pixel_out - m_last_first_pixel_out;
			const auto per_frame = time(span) / static_cast<float>(id_delta);

			if (id_delta == 1 && m_refresh_interval > time_t<std::uint64_t>{}) {
				const auto multiple = std::max(1.f, std::round(per_frame / time(m_refresh_interval)));
				m_interval = time(m_refresh_interval) * multiple;
				m_multiple = static_cast<std::uint32_t>(multiple);
			}
			else {
				m_interval = per_frame;
				m_multiple = 0;
			}

			constexpr float alpha = 0.1f;
			m_smoothed = m_smoothed == time{} ? per_frame : m_smoothed * (1.f - alpha) + per_frame * alpha;
			++m_samples_used;
		}
		if (!m_has_baseline || id != m_last_present_id) {
			m_last_present_id = id;
			m_last_first_pixel_out = first_pixel_out;
			m_has_baseline = true;
		}
	}

	if (++m_observes_since_use > stale_observe_limit) {
		m_interval = time{};
		m_multiple = 0;
	}
}

auto gse::gpu::present_pacer::has_feedback() const -> bool {
	return m_interval > time{};
}

auto gse::gpu::present_pacer::frame_delta() const -> time {
	if (m_interval > time{}) {
		return m_interval;
	}
	if (m_smoothed > time{}) {
		return m_smoothed;
	}
	return time(m_refresh_interval);
}

auto gse::gpu::present_pacer::samples_seen() const -> std::uint64_t {
	return m_samples_seen;
}

auto gse::gpu::present_pacer::samples_used() const -> std::uint64_t {
	return m_samples_used;
}
