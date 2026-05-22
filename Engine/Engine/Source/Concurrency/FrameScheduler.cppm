export module gse.concurrency:frame_scheduler;

import std;

import gse.core;
import gse.meta;
import gse.time;
import gse.log;
import gse.math;

export namespace gse::concurrency {
	class frame_scheduler {
	public:
		template <typename State>
		auto submit(
			move_only_function<void()> work
		) -> void;

		auto flush() -> void;

		auto report_frame_time(
			time fence_wait
		) -> void;

		[[nodiscard]] auto spread() const -> int;

	private:
		struct pending_entry {
			id type;
			gse::move_only_function<void()> work;
		};

		std::vector<pending_entry> m_pending;
		std::unordered_map<id, int> m_registered;

		std::uint64_t m_frame_counter = 0;
		int m_spread = 1;

		static constexpr std::size_t m_history_size = 60;
		std::array<time, m_history_size> m_history{};
		std::size_t m_history_index = 0;

		int m_frames_under_pressure = 0;
		int m_frames_ok = 0;

		static constexpr int m_pressure_frames_to_spread = 2;
		static constexpr int m_ok_frames_to_pack = 120;
		static constexpr int m_max_spread = 4;
	};
}

template <typename State>
auto gse::concurrency::frame_scheduler::submit(move_only_function<void()> work) -> void {
	constexpr id type_id = id_of<State>();

	m_pending.push_back({
		.type = type_id,
		.work = std::move(work),
	});

	if (!m_registered.contains(type_id)) {
		m_registered[type_id] = static_cast<int>(m_registered.size());
	}
}

auto gse::concurrency::frame_scheduler::flush() -> void {
	for (auto& [type, work] : m_pending) {
		if (const int slot = m_registered[type]; m_spread <= 1 || (m_frame_counter % m_spread) == (slot % m_spread)) {
			work();
		}
	}
	m_pending.clear();
	++m_frame_counter;
}

auto gse::concurrency::frame_scheduler::report_frame_time(const time fence_wait) -> void {
	m_history[m_history_index % m_history_size] = fence_wait;
	++m_history_index;

	if (m_history_index < m_history_size) {
		return;
	}

	time median{};
	{
		std::array<time, m_history_size> sorted{};
		std::copy_n(m_history.begin(), m_history_size, sorted.begin());
		std::ranges::sort(sorted);
		median = sorted[m_history_size / 2];
	}

	const time p95 = [&]() -> time {
		std::array<time, m_history_size> sorted{};
		std::copy_n(m_history.begin(), m_history_size, sorted.begin());
		std::ranges::sort(sorted);
		return sorted[m_history_size * 95 / 100];
	}();

	const bool pressure = p95 > median * 2.f && p95 > milliseconds(2.f);

	if (pressure) {
		m_frames_under_pressure++;
		m_frames_ok = 0;
		if (m_frames_under_pressure >= m_pressure_frames_to_spread && m_spread < m_max_spread) {
			++m_spread;
			m_frames_under_pressure = 0;
			log::println(log::category::render, "frame_scheduler: spread increased to {}", m_spread);
		}
	}
	else {
		m_frames_ok++;
		m_frames_under_pressure = 0;
		if (m_frames_ok >= m_ok_frames_to_pack && m_spread > 1) {
			--m_spread;
			m_frames_ok = 0;
			log::println(log::category::render, "frame_scheduler: spread decreased to {}", m_spread);
		}
	}
}

auto gse::concurrency::frame_scheduler::spread() const -> int {
	return m_spread;
}
