export module gse.gpu:present_pacer;

import std;

import gse.gpu_backend;
import gse.math;

export namespace gse::gpu {
	class present_pacer {
	public:
		auto observe(
			std::span<const past_present_timing> samples,
			time_t<std::uint64_t> refresh_interval
		) -> void;

		[[nodiscard]] auto has_feedback() const -> bool;

		[[nodiscard]] auto frame_delta() const -> time;

		[[nodiscard]] auto relative_target() const -> time_t<std::uint64_t>;

	private:
		std::uint64_t m_last_present_id = 0;
		std::uint64_t m_last_first_pixel_out_ns = 0;
		double m_smoothed_ns = 0.0;
		std::uint64_t m_refresh_interval_ns = 0;
		bool m_has_baseline = false;
	};
}

auto gse::gpu::present_pacer::observe(const std::span<const past_present_timing> samples, const time_t<std::uint64_t> refresh_interval) -> void {
	m_refresh_interval_ns = static_cast<std::uint64_t>(refresh_interval);

	std::vector<std::pair<std::uint64_t, std::uint64_t>> points;
	points.reserve(samples.size());
	for (const auto& sample : samples) {
		if (!sample.complete) {
			continue;
		}
		const auto first_pixel_out = static_cast<std::uint64_t>(sample.first_pixel_out);
		if (first_pixel_out != 0) {
			points.emplace_back(sample.present_id, first_pixel_out);
		}
	}
	std::ranges::sort(points);

	for (const auto& [id, first_pixel_out] : points) {
		if (m_has_baseline && id > m_last_present_id) {
			const auto id_delta = id - m_last_present_id;
			const auto time_delta = first_pixel_out - m_last_first_pixel_out_ns;
			const double per_frame = static_cast<double>(time_delta) / static_cast<double>(id_delta);
			constexpr double alpha = 0.1;
			m_smoothed_ns = m_smoothed_ns == 0.0 ? per_frame : m_smoothed_ns * (1.0 - alpha) + per_frame * alpha;
		}
		if (!m_has_baseline || id > m_last_present_id) {
			m_last_present_id = id;
			m_last_first_pixel_out_ns = first_pixel_out;
			m_has_baseline = true;
		}
	}
}

auto gse::gpu::present_pacer::has_feedback() const -> bool {
	return m_smoothed_ns > 0.0;
}

auto gse::gpu::present_pacer::frame_delta() const -> time {
	const auto interval_ns = m_smoothed_ns > 0.0 ? m_smoothed_ns : static_cast<double>(m_refresh_interval_ns);
	return nanoseconds(interval_ns);
}

auto gse::gpu::present_pacer::relative_target() const -> time_t<std::uint64_t> {
	const auto target_ns = m_smoothed_ns > 0.0 ? static_cast<std::uint64_t>(m_smoothed_ns) : m_refresh_interval_ns;
	return time_t<std::uint64_t>(target_ns);
}
