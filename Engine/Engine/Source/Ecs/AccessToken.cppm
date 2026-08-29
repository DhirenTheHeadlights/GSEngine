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
			id type,
			const void* owner
		) -> void;

		auto end_read(
			id type
		) -> void;

		auto begin_write(
			id type,
			const void* owner
		) -> void;

		auto end_write(
			id type
		) -> void;

	private:
		struct slot {
			std::atomic<int> readers{ 0 };
			std::atomic<int> writers{ 0 };
			std::atomic<const void*> writer_owner{ nullptr };
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

	class component_lock : non_copyable {
	public:
		struct config {
			access_guard* guard = nullptr;
			std::atomic<int>* held_locks = nullptr;
			std::vector<id>* authority = nullptr;
			id type;
			access_mode mode = access_mode::read;
		};

		~component_lock();

		component_lock() = default;

		explicit component_lock(
			config cfg
		);

		component_lock(
			component_lock&& other
		) noexcept;

		auto operator=(
			component_lock&& other
		) noexcept -> component_lock&;

	private:
		auto release() -> void;

		config m_config;
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

		struct wiring {
			lookup_fn lookup = nullptr;
			mark_fn mark = nullptr;
			drain_fn drain = nullptr;
			void* ctx = nullptr;
			access_guard* guard = nullptr;
			std::atomic<int>* held_locks = nullptr;
		};

		~access() = default;

		access() = delete;

		access(
			access&& other
		) noexcept = default;

		auto operator=(
			access&& other
		) noexcept -> access& = default;

		[[nodiscard]] auto begin() const -> pointer;

		[[nodiscard]] auto end() const -> pointer;

		[[nodiscard]] auto size() const -> std::size_t;

		[[nodiscard]] auto empty() const -> bool;

		[[nodiscard]] auto data() const -> pointer;

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
			wiring w = {}
		);

		span_type m_span;
		std::span<const id> m_owners;
		lookup_fn m_lookup = nullptr;
		mark_fn m_mark = nullptr;
		drain_fn m_drain = nullptr;
		void* m_lookup_ctx = nullptr;
		component_lock m_lock;
	};

	template <typename T>
	using read = access<T, access_mode::read>;

	template <typename T>
	using write = access<T, access_mode::write>;

	template <typename T>
	class structural : non_copyable {
	public:
		~structural() = default;

		structural() = delete;

		structural(
			structural&& other
		) noexcept = default;

		auto operator=(
			structural&& other
		) noexcept -> structural& = default;

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
		component_lock m_lock;
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
			registry* reg
		);

		registry* m_reg = nullptr;
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

auto gse::access_guard::begin_read(const id type, const void* owner) -> void {
	auto& s = slot_for(type);
	s.readers.fetch_add(1, std::memory_order_acq_rel);
	const bool writer_is_self = s.writer_owner.load(std::memory_order_acquire) == owner;
	assert(s.writers.load(std::memory_order_acquire) == 0 || writer_is_self, "data race: read access on component {} while another system is writing it (missing scheduler dependency)", type);
}

auto gse::access_guard::end_read(const id type) -> void {
	slot_for(type).readers.fetch_sub(1, std::memory_order_acq_rel);
}

auto gse::access_guard::begin_write(const id type, const void* owner) -> void {
	auto& s = slot_for(type);
	const int prev_writers = s.writers.fetch_add(1, std::memory_order_acq_rel);
	const int readers = s.readers.load(std::memory_order_acquire);

	if (prev_writers == 0) {
		s.writer_owner.store(owner, std::memory_order_release);
		assert(readers == 0, "data race: exclusive access on component {} while another system is reading it (missing scheduler dependency)", type);
		return;
	}

	assert(s.writer_owner.load(std::memory_order_acquire) == owner, "data race: exclusive access on component {} while another system is already accessing it (missing scheduler dependency)", type);
}

auto gse::access_guard::end_write(const id type) -> void {
	auto& s = slot_for(type);
	if (s.writers.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		s.writer_owner.store(nullptr, std::memory_order_release);
	}
}

gse::component_lock::component_lock(const config cfg) : m_config(cfg) {
	if (m_config.guard) {
		if (m_config.mode == access_mode::read) {
			m_config.guard->begin_read(m_config.type, m_config.held_locks);
		}
		else {
			m_config.guard->begin_write(m_config.type, m_config.held_locks);
		}
	}
	if (m_config.held_locks) {
		m_config.held_locks->fetch_add(1, std::memory_order_acq_rel);
	}
	if (m_config.guard && m_config.authority) {
		m_config.authority->push_back(m_config.type);
	}
}

gse::component_lock::component_lock(component_lock&& other) noexcept : m_config(std::exchange(other.m_config, config{})) {}

auto gse::component_lock::operator=(component_lock&& other) noexcept -> component_lock& {
	if (this != &other) {
		release();
		m_config = std::exchange(other.m_config, config{});
	}
	return *this;
}

gse::component_lock::~component_lock() {
	release();
}

auto gse::component_lock::release() -> void {
	if (m_config.guard) {
		if (m_config.mode == access_mode::read) {
			m_config.guard->end_read(m_config.type);
		}
		else {
			m_config.guard->end_write(m_config.type);
		}
	}
	if (m_config.held_locks) {
		m_config.held_locks->fetch_sub(1, std::memory_order_acq_rel);
	}
	if (m_config.guard && m_config.authority) {
		if (const auto it = std::ranges::find(*m_config.authority, m_config.type); it != m_config.authority->end()) {
			m_config.authority->erase(it);
		}
	}
}

template <typename T, gse::access_mode M>
gse::access<T, M>::access(const span_type span, const std::span<const id> owners, const wiring w)
	: m_span(span), m_owners(owners), m_lookup(w.lookup), m_mark(w.mark), m_drain(w.drain), m_lookup_ctx(w.ctx) {
	m_lock = component_lock({
		.guard = w.guard,
		.held_locks = w.held_locks,
		.type = id_of<T>(),
		.mode = M,
	});
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::begin() const -> pointer {
	return m_span.data();
}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::end() const -> pointer {
	return m_span.data() + m_span.size();
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
auto gse::access<T, M>::data() const -> pointer {
	return m_span.data();
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
	static_assert(
		M == access_mode::write,
		"drain() consumes the component event queue, so it requires write<T>; systems holding read<T> are not ordered against each other and would each receive an arbitrary subset of the events"
	);
	if (!m_drain) {
		return {};
	}
	return m_drain(m_lookup_ctx, event);
}

template <typename T>
gse::structural<T>::structural(registry* reg, access_guard* guard, std::atomic<int>* held_locks, std::vector<id>* authority) : m_reg(reg) {
	m_lock = component_lock({
		.guard = guard,
		.held_locks = held_locks,
		.authority = authority,
		.type = id_of<T>(),
		.mode = access_mode::write,
	});
}