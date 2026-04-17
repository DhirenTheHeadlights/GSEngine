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
		using const_pointer = const value_type*;
		using reference = value_type&;
		using const_reference = const value_type&;
		using span_type = std::span<value_type>;

		explicit access(
			span_type span
		);

		access(
			access&&
		) noexcept = default;

		auto operator=(
			access&&
		) noexcept -> access& = default;

		~access(
		) override = default;

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
		) const -> const_pointer;

		auto operator[](
			std::size_t i
		) -> reference;

		auto operator[](
			std::size_t i
		) const -> const_reference;

		auto find(
			id owner
		) -> pointer;

		auto find(
			id owner
		) const -> const_pointer;

		auto prime(
		) const -> void;

	private:
		auto build_lookup(
		) -> void;

		span_type m_span;
		mutable std::optional<std::unordered_map<id, pointer>> m_lookup;
	};

	template <is_component T> using read  = access<T, access_mode::read>;
	template <is_component T> using write = access<T, access_mode::write>;
}

template <gse::is_component T, gse::access_mode M>
gse::access<T, M>::access(span_type span) : m_span(span) {}

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
auto gse::access<T, M>::data() const -> const_pointer {
	return m_span.data();
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::operator[](const std::size_t i) -> reference {
	return m_span[i];
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::operator[](const std::size_t i) const -> const_reference {
	return m_span[i];
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::find(const id owner) -> pointer {
	build_lookup();
	if (const auto it = m_lookup->find(owner); it != m_lookup->end()) {
		return it->second;
	}
	return nullptr;
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::find(const id owner) const -> const_pointer {
	const_cast<access*>(this)->build_lookup();
	if (const auto it = m_lookup->find(owner); it != m_lookup->end()) {
		return it->second;
	}
	return nullptr;
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::prime() const -> void {
	const_cast<access*>(this)->build_lookup();
}

template <gse::is_component T, gse::access_mode M>
auto gse::access<T, M>::build_lookup() -> void {
	if (m_lookup) {
		return;
	}
	m_lookup.emplace();
	m_lookup->reserve(m_span.size());
	for (auto& item : m_span) {
		(*m_lookup)[item.owner_id()] = std::addressof(item);
	}
}
