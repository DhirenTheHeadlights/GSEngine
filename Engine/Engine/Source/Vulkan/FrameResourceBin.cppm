export module gse.vulkan:frame_resource_bin;

import std;

import gse.core;

export namespace gse::vulkan {
	enum class queue_id : std::uint8_t {
		graphics = 0,
		compute = 1,
	};

	constexpr std::size_t queue_id_count = 2;

	struct queue_progress {
		queue_id queue;
		std::uint64_t reached_value;
	};

	class frame_resource_bin final : public non_copyable {
	public:
		struct retained_base {
			virtual ~retained_base() = default;
		};

		template <typename T>
		struct retained_holder final : retained_base {
			template <typename U>
			explicit retained_holder(
				U&& v
			) : m_value(std::forward<U>(v)) {
			}

			T m_value;
		};

		frame_resource_bin() {}
		~frame_resource_bin();

		frame_resource_bin(
			frame_resource_bin&&
		) noexcept = default;

		auto operator=(
			frame_resource_bin&&
		) noexcept -> frame_resource_bin& = default;

		template <typename T>
		auto retain(
			queue_id queue,
			std::uint64_t until_value,
			T resource
		) -> void;

		auto retain(
			queue_id queue,
			std::uint64_t until_value,
			std::unique_ptr<retained_base> resource
		) -> void;

		auto drain(
			std::span<const queue_progress> progress
		) -> void;

		auto wait_idle_clear() -> void;

		[[nodiscard]] auto pending_count() const -> std::size_t;

	private:
		struct slot {
			queue_id m_queue;
			std::uint64_t m_until_value;
			std::unique_ptr<retained_base> m_holder;
		};

		std::vector<slot> m_slots;
		std::unique_ptr<std::mutex> m_mutex = std::make_unique<std::mutex>();
	};
}

gse::vulkan::frame_resource_bin::~frame_resource_bin() = default;

template <typename T>
auto gse::vulkan::frame_resource_bin::retain(const queue_id queue, const std::uint64_t until_value, T resource) -> void {
	retain(queue, until_value, std::make_unique<retained_holder<T>>(std::move(resource)));
}

auto gse::vulkan::frame_resource_bin::retain(const queue_id queue, const std::uint64_t until_value, std::unique_ptr<retained_base> resource) -> void {
	std::lock_guard lock(*m_mutex);
	m_slots.push_back({
		.m_queue = queue,
		.m_until_value = until_value,
		.m_holder = std::move(resource),
	});
}

auto gse::vulkan::frame_resource_bin::drain(std::span<const queue_progress> progress) -> void {
	auto reached = [progress](const queue_id q) -> std::uint64_t {
		for (const auto& p : progress) {
			if (p.queue == q) {
				return p.reached_value;
			}
		}
		return 0;
	};

	std::vector<slot> retired;
	{
		std::lock_guard lock(*m_mutex);
		for (auto it = m_slots.begin(); it != m_slots.end();) {
			if (it->m_until_value <= reached(it->m_queue)) {
				retired.push_back(std::move(*it));
				it = m_slots.erase(it);
			}
			else {
				++it;
			}
		}
	}
}

auto gse::vulkan::frame_resource_bin::wait_idle_clear() -> void {
	std::lock_guard lock(*m_mutex);
	m_slots.clear();
}

auto gse::vulkan::frame_resource_bin::pending_count() const -> std::size_t {
	std::lock_guard lock(*m_mutex);
	return m_slots.size();
}
