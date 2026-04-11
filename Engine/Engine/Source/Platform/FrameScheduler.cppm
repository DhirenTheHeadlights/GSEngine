export module gse.platform:frame_scheduler;

import std;

import gse.math;
import gse.log;

export namespace gse::gpu {
	class frame_scheduler {
	public:
		template <typename State>
		auto submit(std::move_only_function<void()> work) -> void {
			m_pending.push_back({
				.type = std::type_index(typeid(State)),
				.work = std::move(work)
			});

			if (!m_registered.contains(std::type_index(typeid(State)))) {
				m_registered[std::type_index(typeid(State))] = static_cast<int>(m_registered.size());
			}
		}

		auto flush() -> void {
			for (auto& entry : m_pending) {
				const int slot = m_registered[entry.type];
				if (m_spread <= 1 || (m_frame_counter % m_spread) == (slot % m_spread)) {
					entry.work();
				}
			}
			m_pending.clear();
			++m_frame_counter;
		}

		auto report_frame_time(time_t<float> fence_wait) -> void {
			const float ms = fence_wait.as<milliseconds>();

			m_history[m_history_index % m_history_size] = ms;
			++m_history_index;

			if (m_history_index < m_history_size) {
				return;
			}

			float median = 0.f;
			{
				std::array<float, m_history_size> sorted{};
				std::copy_n(m_history.begin(), m_history_size, sorted.begin());
				std::ranges::sort(sorted);
				median = sorted[m_history_size / 2];
			}

			const float p95 = [&] {
				std::array<float, m_history_size> sorted{};
				std::copy_n(m_history.begin(), m_history_size, sorted.begin());
				std::ranges::sort(sorted);
				return sorted[m_history_size * 95 / 100];
			}();

			const bool pressure = p95 > median * 2.f && p95 > 2.f;

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

		auto spread() const -> int { return m_spread; }

	private:
		struct pending_entry {
			std::type_index type = typeid(void);
			std::move_only_function<void()> work;
		};

		std::vector<pending_entry> m_pending;
		std::unordered_map<std::type_index, int> m_registered;

		std::uint64_t m_frame_counter = 0;
		int m_spread = 1;

		static constexpr std::size_t m_history_size = 60;
		std::array<float, m_history_size> m_history{};
		std::size_t m_history_index = 0;

		int m_frames_under_pressure = 0;
		int m_frames_ok = 0;

		static constexpr int m_pressure_frames_to_spread = 2;
		static constexpr int m_ok_frames_to_pack = 120;
		static constexpr int m_max_spread = 4;
	};
}
