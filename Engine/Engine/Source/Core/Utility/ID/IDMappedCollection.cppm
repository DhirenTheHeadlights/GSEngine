export module gse.utility:id_mapped_collection;

import std;

export namespace gse {
	template <typename T, typename PrimaryIdType>
	class id_mapped_collection_t {
	public:
		auto add(
			const PrimaryIdType& id,
			T object
		) -> T*;

		auto remove(
			const PrimaryIdType& id
		) -> void;

		auto pop(
			const PrimaryIdType& id
		) -> std::optional<T>;

		auto try_get(
			const PrimaryIdType& id
		) -> T*;

		auto try_get(
			const PrimaryIdType& id
		) const -> const T*;

		auto items(
		) -> std::span<T>;

		auto contains(
			const PrimaryIdType& id
		) const -> bool;

		auto size(
		) const -> size_t;

		auto clear(
		) noexcept -> void;
	private:
		std::vector<T> m_items;
		std::vector<PrimaryIdType> m_ids;
		std::unordered_map<PrimaryIdType, size_t> m_map;
	};
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::add(const PrimaryIdType& id, T object) -> T* {
	if (m_map.contains(id)) {
		return nullptr;
	}

	const std::size_t new_index = m_items.size();
	m_map[id] = new_index;
	m_ids.push_back(id);
	return &m_items.emplace_back(std::move(object));
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::remove(const PrimaryIdType& id) -> void {
	const auto it = m_map.find(id);
	if (it == m_map.end()) {
		return;
	}

	const size_t index_to_remove = it->second;

	if (const size_t last_index = m_items.size() - 1; index_to_remove != last_index) {
		const PrimaryIdType& last_id = m_ids.back();
		m_items[index_to_remove] = std::move(m_items.back());
		m_ids[index_to_remove] = std::move(m_ids.back());
		m_map[last_id] = index_to_remove;
	}

	m_map.erase(id);
	m_items.pop_back();
	m_ids.pop_back();
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::pop(const PrimaryIdType& id) -> std::optional<T> {
	const auto it = m_map.find(id);
	if (it == m_map.end()) {
		return std::nullopt;
	}

	const size_t index_to_pop = it->second;
	T popped_object = std::move(m_items[index_to_pop]);

	remove(id);

	return popped_object;
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::try_get(const PrimaryIdType& id) -> T* {
	if (const auto it = m_map.find(id); it != m_map.end()) {
		return &m_items[it->second];
	}
	return nullptr;
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::try_get(const PrimaryIdType& id) const -> const T* {
	if (const auto it = m_map.find(id); it != m_map.end()) {
		return &m_items[it->second];
	}
	return nullptr;
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::items() -> std::span<T> {
	return m_items;
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::contains(const PrimaryIdType& id) const -> bool {
	return m_map.contains(id);
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::size() const -> size_t {
	return m_items.size();
}

template <typename T, typename PrimaryIdType>
auto gse::id_mapped_collection_t<T, PrimaryIdType>::clear() noexcept -> void {
	m_items.clear();
	m_ids.clear();
	m_map.clear();
}