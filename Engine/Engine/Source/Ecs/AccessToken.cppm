export module gse.ecs:access_token;

import std;

import gse.assert;
import gse.core;

import :component;

export namespace gse {
	enum class access_mode : std::uint8_t {
		read,
		write
	};

	class registry;
	class context;

	class access_token : non_copyable {
	public:
		access_token(
			access_token&&
		) noexcept = default;

		auto operator=(
			access_token&&
		) noexcept -> access_token& = default;

	private:
		friend class context;
		access_token() = default;
	};

	class access_guard {
	public:
		auto begin_read(
			id type
		) -> void;

		auto end_read(
			id type
		) -> void;

		auto begin_write(
			id type
		) -> void;

		auto end_write(
			id type
		) -> void;

	private:
		struct slot {
			std::atomic<int> readers{ 0 };
			std::atomic<int> writers{ 0 };
		};

		auto slot_for(
			id type
		) -> slot&;

		std::unordered_map<id, std::unique_ptr<slot>> m_slots;
		std::shared_mutex m_map_mutex;
	};

	enum class component_event : std::uint8_t {
		added,
		updated,
		removed
	};

	template <typename T, access_mode M = access_mode::read>
	class access : non_copyable {
	public:
		using value_type = std::conditional_t<M == access_mode::read, const T, T>;
		using pointer = value_type*;
		using reference = value_type&;
		using span_type = std::span<value_type>;
		using lookup_fn = pointer (
				*
		)(
			void* ctx,
			id
		);
		using mark_fn = void (
				*
		)(
			void* ctx,
			id
		);
		using drain_fn = std::vector<id> (
				*
		)(
			void* ctx,
			component_event
		);

		~access();

		access() = delete;

		access(
			access&& other
		) noexcept;

		auto operator=(
			access&& other
		) noexcept -> access&;

		auto begin(
			this access& self
		) -> decltype(auto);

		auto end(
			this access& self
		) -> decltype(auto);

		[[nodiscard]] auto size() const -> std::size_t;

		[[nodiscard]] auto empty() const -> bool;

		auto data() -> pointer;

		auto data() const -> pointer;

		auto operator[](
			std::size_t i
		) -> reference;

		auto operator[](
			std::size_t i
		) const -> reference;

		auto find(
			id owner
		) const -> pointer;

		[[nodiscard]] auto owner_ids() const -> std::span<const id>;

		[[nodiscard]] auto owner_id_at(
			std::size_t i
		) const -> id;

		auto mark_updated(
			id owner
		) const -> void;

		auto drain(
			component_event event
		) const -> std::vector<id>;

	private:
		friend class registry;

		explicit access(
			span_type span,
			std::span<const id> owners,
			lookup_fn fn = nullptr,
			mark_fn mark = nullptr,
			drain_fn drain = nullptr,
			void* ctx = nullptr,
			access_guard* guard = nullptr,
			std::atomic<int>* held_locks = nullptr
		);

		span_type m_span;
		std::span<const id> m_owners;
		lookup_fn m_lookup = nullptr;
		mark_fn m_mark = nullptr;
		drain_fn m_drain = nullptr;
		void* m_lookup_ctx = nullptr;
		access_guard* m_guard = nullptr;
		std::atomic<int>* m_held_locks = nullptr;
	};

	template <typename T>
	using read = access<T, access_mode::read>;

	template <typename T>
	using write = access<T, access_mode::write>;

	template <typename T>
	class structural : non_copyable {
	public:
		~structural();

		structural() = delete;

		structural(
			structural&& other
		) noexcept;

		auto operator=(
			structural&& other
		) noexcept -> structural&;

		auto add(
			id owner,
			T value = T{}
		) const -> T*;

		auto remove(
			id owner
		) const -> void;

		auto ensure_storage() const -> void;

		[[nodiscard]] auto contains(
			id owner
		) const -> bool;

	private:
		friend class context;

		structural(
			registry* reg,
			access_guard* guard,
			std::atomic<int>* held_locks,
			std::vector<id>* authority
		);

		registry* m_reg = nullptr;
		access_guard* m_guard = nullptr;
		std::atomic<int>* m_held_locks = nullptr;
		std::vector<id>* m_authority = nullptr;
	};

	class entities {
	public:
		auto ensure_exists(
			id owner
		) const -> void;

		[[nodiscard]] auto exists(
			id owner
		) const -> bool;

		[[nodiscard]] auto active(
			id owner
		) const -> bool;

		auto ensure_active(
			id owner
		) const -> void;

		auto remove(
			id owner
		) const -> void;

	private:
		friend class context;

		explicit entities(
			gse::registry* reg
		);

		gse::registry* m_reg = nullptr;
	};
}

auto gse::access_guard::slot_for(const id type) -> slot& {
	{
		std::shared_lock lock(m_map_mutex);
		if (const auto it = m_slots.find(type); it != m_slots.end()) {
			return *it->second;
		}
	}
	std::unique_lock lock(m_map_mutex);
	if (const auto it = m_slots.find(type); it != m_slots.end()) {
		return *it->second;
	}
	auto fresh = std::make_unique<slot>();
	auto& ref = *fresh;
	m_slots.emplace(type, std::move(fresh));
	return ref;
}

auto gse::access_guard::begin_read(const id type) -> void {
	auto& s = slot_for(type);
	s.readers.fetch_add(1, std::memory_order_acq_rel);
	assert(s.writers.load(std::memory_order_acquire) == 0, "data race: read access on component {} while a writer is active in the same tick (missing scheduler dependency)", type);
}

auto gse::access_guard::end_read(const id type) -> void {
	slot_for(type).readers.fetch_sub(1, std::memory_order_acq_rel);
}

auto gse::access_guard::begin_write(const id type) -> void {
	auto& s = slot_for(type);
	const int prev_writers = s.writers.fetch_add(1, std::memory_order_acq_rel);
	const int readers = s.readers.load(std::memory_order_acquire);
	assert(prev_writers == 0 && readers == 0, "data race: exclusive access on component {} while it is already being accessed in the same tick (missing scheduler dependency)", type);
}

auto gse::access_guard::end_write(const id type) -> void {
	slot_for(type).writers.fetch_sub(1, std::memory_order_acq_rel);
}

template <typename T, gse::access_mode M>
gse::access<T, M>::access(const span_type span, const std::span<const id> owners, const lookup_fn fn, const mark_fn mark, const drain_fn drain, void* ctx, access_guard* guard, std::atomic<int>* held_locks)
	: m_span(span), m_owners(owners), m_lookup(fn), m_mark(mark), m_drain(drain), m_lookup_ctx(ctx), m_guard(guard), m_held_locks(held_locks) {
	if (m_guard) {
		if constexpr (M == access_mode::read) {
			m_guard->begin_read(id_of<T>());
		}
		else {
			m_guard->begin_write(id_of<T>());
		}
	}
	if (m_held_locks) {
		m_held_locks->fetch_add(1, std::memory_order_acq_rel);
	}
}

template <typename T, gse::access_mode M>
gse::access<T, M>::access(access&& other) noexcept
	: m_span(other.m_span), m_owners(other.m_owners), m_lookup(other.m_lookup), m_lookup_ctx(other.m_lookup_ctx), m_guard(std::exchange(other.m_guard, nullptr)), m_held_locks(std::exchange(other.m_held_locks, nullptr)) {
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::operator=(access&& other) noexcept -> access& {
	if (this != &other) {
		if (m_guard) {
			if constexpr (M == access_mode::read) {
				m_guard->end_read(id_of<T>());
			}
			else {
				m_guard->end_write(id_of<T>());
			}
		}
		if (m_held_locks) {
			m_held_locks->fetch_sub(1, std::memory_order_acq_rel);
		}
		m_span = other.m_span;
		m_owners = other.m_owners;
		m_lookup = other.m_lookup;
		m_lookup_ctx = other.m_lookup_ctx;
		m_guard = std::exchange(other.m_guard, nullptr);
		m_held_locks = std::exchange(other.m_held_locks, nullptr);
	}
	return *this;
}

template <typename T, gse::access_mode M>
gse::access<T, M>::~access() {
	if (m_guard) {
		if constexpr (M == access_mode::read) {
			m_guard->end_read(id_of<T>());
		}
		else {
			m_guard->end_write(id_of<T>());
		}
	}
	if (m_held_locks) {
		m_held_locks->fetch_sub(1, std::memory_order_acq_rel);
	}
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::begin(this access& self) -> decltype(auto) {
	return self.m_span.data();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::end(this access& self) -> decltype(auto) {
	return self.m_span.data() + self.m_span.size();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::size() const -> std::size_t {
	return m_span.size();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::empty() const -> bool {
	return m_span.empty();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::data() -> pointer {
	return m_span.data();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::data() const -> pointer {
	return m_span.data();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::operator[](const std::size_t i) -> reference {
	return m_span[i];
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::operator[](const std::size_t i) const -> reference {
	return m_span[i];
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::find(const id owner) const -> pointer {
	if (!m_lookup) {
		return nullptr;
	}
	return m_lookup(m_lookup_ctx, owner);
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::owner_ids() const -> std::span<const id> {
	return m_owners;
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::owner_id_at(const std::size_t i) const -> id {
	return m_owners[i];
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::mark_updated(const id owner) const -> void {
	if (!m_mark) {
		return;
	}
	m_mark(m_lookup_ctx, owner);
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::drain(const component_event event) const -> std::vector<id> {
	if (!m_drain) {
		return {};
	}
	return m_drain(m_lookup_ctx, event);
}

template <typename T>
gse::structural<T>::structural(registry* reg, access_guard* guard, std::atomic<int>* held_locks, std::vector<id>* authority)
	: m_reg(reg), m_guard(guard), m_held_locks(held_locks), m_authority(authority) {
	if (m_guard) {
		m_guard->begin_write(id_of<T>());
	}
	if (m_held_locks) {
		m_held_locks->fetch_add(1, std::memory_order_acq_rel);
	}
	if (m_guard && m_authority) {
		m_authority->push_back(id_of<T>());
	}
}

template <typename T>
gse::structural<T>::structural(structural&& other) noexcept
	: m_reg(other.m_reg), m_guard(std::exchange(other.m_guard, nullptr)), m_held_locks(std::exchange(other.m_held_locks, nullptr)), m_authority(std::exchange(other.m_authority, nullptr)) {
}

template <typename T>
auto gse::structural<T>::operator=(structural&& other) noexcept -> structural& {
	if (this != &other) {
		if (m_guard) {
			m_guard->end_write(id_of<T>());
		}
		if (m_held_locks) {
			m_held_locks->fetch_sub(1, std::memory_order_acq_rel);
		}
		if (m_guard && m_authority) {
			if (const auto it = std::ranges::find(*m_authority, id_of<T>()); it != m_authority->end()) {
				m_authority->erase(it);
			}
		}
		m_reg = other.m_reg;
		m_guard = std::exchange(other.m_guard, nullptr);
		m_held_locks = std::exchange(other.m_held_locks, nullptr);
		m_authority = std::exchange(other.m_authority, nullptr);
	}
	return *this;
}

template <typename T>
gse::structural<T>::~structural() {
	if (m_guard) {
		m_guard->end_write(id_of<T>());
	}
	if (m_held_locks) {
		m_held_locks->fetch_sub(1, std::memory_order_acq_rel);
	}
	if (m_guard && m_authority) {
		if (const auto it = std::ranges::find(*m_authority, id_of<T>()); it != m_authority->end()) {
			m_authority->erase(it);
		}
	}
}
