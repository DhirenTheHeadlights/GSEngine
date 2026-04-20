export module gse.utility:access_token;

import std;

import :concepts;
import :id;
import :non_copyable;

export namespace gse {
	enum class access_mode { read, write };

	template <is_component T, access_mode M = access_mode::read>
	class access : non_copyable {
	public:
		using value_type = std::conditional_t<M == access_mode::read, const T, T>;
		using pointer = value_type*;
		using reference = value_type&;
		using span_type = std::span<value_type>;
		using lookup_fn = pointer (*)(void* ctx, id);

		~access(
		) override = default;

		access(
		) = default;

		explicit access(
			span_type span,
			lookup_fn fn = nullptr,
			void* ctx = nullptr
		);

		access(
			access&&
		) noexcept = default;

		auto operator=(
			access&&
		) noexcept -> access& = default;

		template <typename Self>
		auto begin(
			this Self& self
		) -> decltype(auto);

		template <typename Self>
		auto end(
			this Self& self
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

	private:
		span_type m_span;
		lookup_fn m_lookup = nullptr;
		void* m_lookup_ctx = nullptr;
	};

	template <is_component T> using read  = access<T, access_mode::read>;
	template <is_component T> using write = access<T, access_mode::write>;
}

template <gse::is_component T, gse::access_mode M>
gse::access<T, M>::access(span_type span, lookup_fn fn, void* ctx) : m_span(span), m_lookup(fn), m_lookup_ctx(ctx) {}

template <gse::is_component T, gse::access_mode M>
template <typename Self>
auto gse::access<T, M>::begin(this Self& self) -> decltype(auto) {
	return self.m_span.data();
}

template <gse::is_component T, gse::access_mode M>
template <typename Self>
auto gse::access<T, M>::end(this Self& self) -> decltype(auto) {
	return self.m_span.data() + self.m_span.size();
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::size() const -> std::size_t {
	return m_span.size();
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::empty() const -> bool {
	return m_span.empty();
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::data() -> pointer {
	return m_span.data();
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::data() const -> pointer {
	return m_span.data();
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::operator[](const std::size_t i) -> reference {
	return m_span[i];
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::operator[](const std::size_t i) const -> reference {
	return m_span[i];
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::find(const id owner) const -> pointer {
	if (!m_lookup) {
		return nullptr;
	}
	return m_lookup(m_lookup_ctx, owner);
}
