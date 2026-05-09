export module gse.ecs:access_token;

import std;

import gse.core;
import gse.concurrency;

import :component;

export namespace gse {
	enum class access_mode { read, write };

	class registry;
	class run_context;

	class access_token {
	public:
		access_token(
			access_token&&
		) noexcept = default;

		auto operator=(
			access_token&&
		) noexcept -> access_token& = default;

		access_token(
			const access_token&
		) = delete;

		auto operator=(
			const access_token&
		) -> access_token& = delete;

	private:
		friend class run_context;
		access_token() = default;
	};

	template <typename T, access_mode M = access_mode::read>
	class access : non_copyable {
	public:
		using value_type = std::conditional_t<M == access_mode::read, const T, T>;
		using pointer = value_type*;
		using reference = value_type&;
		using span_type = std::span<value_type>;
		using lookup_fn = pointer (*)(void* ctx, id);

		~access(
		) override;

		access(
		) = delete;

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

		auto size(
		) const -> std::size_t;

		auto empty(
		) const -> bool;

		auto data(
		) -> pointer;

		auto data(
		) const -> pointer;

		auto operator[](
			std::size_t i
		) -> reference;

		auto operator[](
			std::size_t i
		) const -> reference;

		auto find(
			id owner
		) const -> pointer;

		auto owner_ids(
		) const -> std::span<const id>;

		auto owner_id_at(
			std::size_t i
		) const -> id;

	private:
		friend class registry;

		explicit access(
			span_type span,
			std::span<const id> owners,
			lookup_fn fn = nullptr,
			void* ctx = nullptr,
			async::rw_mutex* mutex = nullptr,
			std::atomic<int>* held_locks = nullptr
		);

		span_type m_span;
		std::span<const id> m_owners;
		lookup_fn m_lookup = nullptr;
		void* m_lookup_ctx = nullptr;
		async::rw_mutex* m_mutex = nullptr;
		std::atomic<int>* m_held_locks = nullptr;
	};

	template <typename T>
	using read = access<T, access_mode::read>;

	template <typename T>
	using write = access<T, access_mode::write>;
}

template <typename T, gse::access_mode M>
gse::access<T, M>::access(const span_type span, const std::span<const id> owners, const lookup_fn fn, void* ctx, async::rw_mutex* mutex, std::atomic<int>* held_locks)
	: m_span(span), m_owners(owners), m_lookup(fn), m_lookup_ctx(ctx), m_mutex(mutex), m_held_locks(held_locks) {
	if (m_mutex && m_held_locks) {
		m_held_locks->fetch_add(1, std::memory_order_acq_rel);
	}
}

template <typename T, gse::access_mode M>
gse::access<T, M>::access(access&& other) noexcept
	: m_span(other.m_span),
	  m_owners(other.m_owners),
	  m_lookup(other.m_lookup),
	  m_lookup_ctx(other.m_lookup_ctx),
	  m_mutex(std::exchange(other.m_mutex, nullptr)),
	  m_held_locks(std::exchange(other.m_held_locks, nullptr)) {}

template <typename T, gse::access_mode M>
auto gse::access<T, M>::operator=(access&& other) noexcept -> access& {
	if (this != &other) {
		if (m_mutex) {
			if constexpr (M == access_mode::read) {
				m_mutex->unlock_shared();
			}
			else {
				m_mutex->unlock_exclusive();
			}
			if (m_held_locks) {
				m_held_locks->fetch_sub(1, std::memory_order_acq_rel);
			}
		}
		m_span = other.m_span;
		m_owners = other.m_owners;
		m_lookup = other.m_lookup;
		m_lookup_ctx = other.m_lookup_ctx;
		m_mutex = std::exchange(other.m_mutex, nullptr);
		m_held_locks = std::exchange(other.m_held_locks, nullptr);
	}
	return *this;
}

template <typename T, gse::access_mode M>
gse::access<T, M>::~access() {
	if (!m_mutex) {
		return;
	}
	if constexpr (M == access_mode::read) {
		m_mutex->unlock_shared();
	}
	else {
		m_mutex->unlock_exclusive();
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
