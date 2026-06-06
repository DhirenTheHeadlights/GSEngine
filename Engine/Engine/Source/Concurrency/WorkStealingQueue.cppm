export module gse.concurrency:work_stealing_queue;

import std;

import gse.assert;
import gse.core;

export namespace gse::task {
	template <typename T>
	class work_stealing_queue : non_copyable, non_movable {
	public:
		explicit work_stealing_queue(
			std::size_t initial_capacity = 256
		);

		~work_stealing_queue();

		auto push(
			T value
		) -> void;

		auto try_pop(
			T& out
		) -> bool;

		auto try_steal(
			T& out
		) -> bool;

		[[nodiscard]] auto approximate_size() const noexcept -> std::size_t;

	private:
		struct slot {
			alignas(
				T
			) std::byte m_storage[sizeof(T)]{};
			std::atomic<bool> m_occupied{ false };

			auto value() noexcept -> T*;

			auto value() const noexcept -> const T*;
		};

		class buffer : non_copyable, non_movable {
		public:
			explicit buffer(
				std::size_t capacity
			);

			~buffer();

			[[nodiscard]] auto capacity() const noexcept -> std::size_t;

			[[nodiscard]] auto occupied(
				std::size_t index
			) const noexcept -> bool;

			auto construct(
				std::size_t index,
				T&& value
			) -> void;

			auto take(
				std::size_t index,
				T& out
			) -> void;

			auto move_construct_from(
				std::size_t index,
				buffer& source
			) -> void;

		private:
			[[nodiscard]] auto slot_at(
				std::size_t index
			) noexcept -> slot&;

			[[nodiscard]] auto slot_at(
				std::size_t index
			) const noexcept -> const slot&;

			std::size_t m_capacity = 0;
			std::size_t m_mask = 0;
			std::unique_ptr<slot[]> m_slots;
		};

		[[nodiscard]] static auto rounded_capacity(
			std::size_t requested
		) -> std::size_t;

		[[nodiscard]]
		auto needs_growth(
			const buffer& current,
			std::size_t top,
			std::size_t bottom
		) const noexcept -> bool;

		auto grow(
			std::size_t bottom
		) -> buffer&;

		static constexpr std::size_t cache_line_size = 64;

		std::unique_ptr<buffer> m_current;
		std::vector<std::unique_ptr<buffer>> m_retired;
		std::atomic<buffer*> m_buffer{ nullptr };
		mutable std::shared_mutex m_resize_mutex;
		alignas(cache_line_size) std::atomic<std::size_t> m_top{ 0 };
		alignas(cache_line_size) std::atomic<std::size_t> m_bottom{ 0 };
	};
}

template <typename T>
auto gse::task::work_stealing_queue<T>::slot::value() noexcept -> T* {
	return std::launder(reinterpret_cast<T*>(m_storage));
}

template <typename T>
auto gse::task::work_stealing_queue<T>::slot::value() const noexcept -> const T* {
	return std::launder(reinterpret_cast<const T*>(m_storage));
}

template <typename T>
gse::task::work_stealing_queue<T>::buffer::buffer(const std::size_t capacity)
	: m_capacity(capacity), m_mask(capacity - 1), m_slots(std::make_unique<slot[]>(capacity)) {
	assert((capacity & (capacity - 1)) == 0, "work_stealing_queue buffer capacity must be a power of two");
}

template <typename T>
gse::task::work_stealing_queue<T>::buffer::~buffer() {
	for (std::size_t i = 0; i < m_capacity; ++i) {
		auto& current = m_slots[i];
		if (current.m_occupied.load(std::memory_order_acquire)) {
			std::destroy_at(current.value());
		}
	}
}

template <typename T>
auto gse::task::work_stealing_queue<T>::buffer::capacity() const noexcept -> std::size_t {
	return m_capacity;
}

template <typename T>
auto gse::task::work_stealing_queue<T>::buffer::occupied(const std::size_t index) const noexcept -> bool {
	return slot_at(index).m_occupied.load(std::memory_order_acquire);
}

template <typename T>
auto gse::task::work_stealing_queue<T>::buffer::construct(const std::size_t index, T&& value) -> void {
	auto& target = slot_at(index);
	assert(!target.m_occupied.load(std::memory_order_acquire), "work_stealing_queue constructed into an occupied slot");
	std::construct_at(target.value(), std::move(value));
	target.m_occupied.store(true, std::memory_order_release);
}

template <typename T>
auto gse::task::work_stealing_queue<T>::buffer::take(const std::size_t index, T& out) -> void {
	auto& source = slot_at(index);
	assert(source.m_occupied.load(std::memory_order_acquire), "work_stealing_queue took from an empty slot");
	out = std::move(*source.value());
	std::destroy_at(source.value());
	source.m_occupied.store(false, std::memory_order_release);
}

template <typename T>
auto gse::task::work_stealing_queue<T>::buffer::move_construct_from(const std::size_t index, buffer& source) -> void {
	auto& source_slot = source.slot_at(index);
	auto& target_slot = slot_at(index);
	assert(
		source_slot.m_occupied.load(std::memory_order_acquire),
		"work_stealing_queue resized from an empty source slot"
	);
	assert(
		!target_slot.m_occupied.load(std::memory_order_acquire),
		"work_stealing_queue resized into an occupied target slot"
	);
	std::construct_at(target_slot.value(), std::move(*source_slot.value()));
	target_slot.m_occupied.store(true, std::memory_order_release);
	std::destroy_at(source_slot.value());
	source_slot.m_occupied.store(false, std::memory_order_release);
}

template <typename T>
auto gse::task::work_stealing_queue<T>::buffer::slot_at(const std::size_t index) noexcept -> slot& {
	return m_slots[index & m_mask];
}

template <typename T>
auto gse::task::work_stealing_queue<T>::buffer::slot_at(const std::size_t index) const noexcept -> const slot& {
	return m_slots[index & m_mask];
}

template <typename T>
gse::task::work_stealing_queue<T>::work_stealing_queue(const std::size_t initial_capacity)
	: m_current(std::make_unique<buffer>(rounded_capacity(initial_capacity))) {
	m_buffer.store(m_current.get(), std::memory_order_release);
}

template <typename T>
gse::task::work_stealing_queue<T>::~work_stealing_queue() = default;

template <typename T>
auto gse::task::work_stealing_queue<T>::push(T value) -> void {
	const auto bottom = m_bottom.load(std::memory_order_relaxed);
	const auto top = m_top.load(std::memory_order_acquire);
	auto* current = m_buffer.load(std::memory_order_acquire);
	if (needs_growth(*current, top, bottom)) {
		current = std::addressof(grow(bottom));
	}
	current->construct(bottom, std::move(value));
	m_bottom.store(bottom + 1, std::memory_order_release);
}

template <typename T>
auto gse::task::work_stealing_queue<T>::try_pop(T& out) -> bool {
	const auto original_bottom = m_bottom.load(std::memory_order_relaxed);
	if (original_bottom == m_top.load(std::memory_order_acquire)) {
		return false;
	}

	const auto bottom = original_bottom - 1;
	m_bottom.store(bottom, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_seq_cst);

	auto top = m_top.load(std::memory_order_relaxed);
	if (top > bottom) {
		m_bottom.store(original_bottom, std::memory_order_release);
		return false;
	}

	auto* current = m_buffer.load(std::memory_order_acquire);
	if (top == bottom) {
		if (!m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			m_bottom.store(original_bottom, std::memory_order_release);
			return false;
		}
		current->take(bottom, out);
		m_bottom.store(original_bottom, std::memory_order_release);
		return true;
	}

	current->take(bottom, out);
	return true;
}

template <typename T>
auto gse::task::work_stealing_queue<T>::try_steal(T& out) -> bool {
	const std::shared_lock lock(m_resize_mutex);
	auto top = m_top.load(std::memory_order_acquire);
	std::atomic_thread_fence(std::memory_order_seq_cst);
	const auto bottom = m_bottom.load(std::memory_order_acquire);
	if (top >= bottom) {
		return false;
	}

	auto* current = m_buffer.load(std::memory_order_acquire);
	if (!m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
		return false;
	}

	current->take(top, out);
	return true;
}

template <typename T>
auto gse::task::work_stealing_queue<T>::approximate_size() const noexcept -> std::size_t {
	const auto top = m_top.load(std::memory_order_acquire);
	const auto bottom = m_bottom.load(std::memory_order_acquire);
	if (bottom <= top) {
		return 0;
	}
	return bottom - top;
}

template <typename T>
auto gse::task::work_stealing_queue<T>::rounded_capacity(std::size_t requested) -> std::size_t {
	requested = std::max<std::size_t>(requested, 2);
	std::size_t result = 2;
	while (result < requested) {
		assert(result <= std::numeric_limits<std::size_t>::max() / 2, "work_stealing_queue capacity overflow");
		result *= 2;
	}
	return result;
}

template <typename T>
auto gse::task::work_stealing_queue<T>::needs_growth(const buffer& current, const std::size_t top, const std::size_t bottom) const noexcept -> bool {
	return bottom - top >= current.capacity() - 1 || current.occupied(bottom);
}

template <typename T>
auto gse::task::work_stealing_queue<T>::grow(const std::size_t bottom) -> buffer& {
	std::unique_lock lock(m_resize_mutex);
	auto* current = m_buffer.load(std::memory_order_acquire);
	const auto top = m_top.load(std::memory_order_acquire);
	if (!needs_growth(*current, top, bottom)) {
		return *current;
	}

	auto next = std::make_unique<buffer>(current->capacity() * 2);
	for (std::size_t i = top; i < bottom; ++i) {
		next->move_construct_from(i, *current);
	}

	auto* raw = next.get();
	m_retired.push_back(std::move(m_current));
	m_current = std::move(next);
	m_buffer.store(raw, std::memory_order_release);
	return *raw;
}
