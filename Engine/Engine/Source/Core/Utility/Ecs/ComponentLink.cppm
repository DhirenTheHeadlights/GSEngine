export module gse.utility:component_link;

import std;

import :id;
import :component;
import :misc;
import :frame_sync;
import :concepts;

export namespace gse {
	class component_link_base {
	public:
		virtual ~component_link_base() = default;

		virtual auto activate(
			id id
		) -> bool = 0;

		virtual auto remove(
			id id
		) -> void = 0;

		virtual auto bind(
			std::size_t read_index,
			std::size_t write_index
		) -> void = 0;

		virtual auto flip(
			std::size_t read,
			std::size_t write
		) -> void = 0;
	};
}

export namespace gse {
	class registry;

	template <is_component T>
	class component_link final : public component_link_base {
	public:
		using owner_id_t = id;
		using link_id_t = id;
		using component_type = T;
		using dbq = double_buffered_id_mapped_queue<component_type, owner_id_t>;

		template <typename... Args>
		auto add(
			owner_id_t owner_id,
			registry* reg,
			Args&&... args
		) -> T*;

		auto activate(
			owner_id_t owner_id
		) -> bool override;

		auto remove(
			owner_id_t owner_id
		) -> void override;

		auto objects_read(
		) -> std::span<const component_type>;

		auto objects_write(
		) -> std::span<component_type>;

		auto try_get_read(
			owner_id_t owner_id
		) -> const component_type*;

		auto try_get_write(
			owner_id_t owner_id
		) -> component_type*;

		auto try_get_by_link_id_read(
			const link_id_t& link_id
		) -> const T*;

		auto try_get_by_link_id_write(
			const link_id_t& link_id
		) -> T*;

		auto bind(
			std::size_t read_index,
			std::size_t write_index
		) -> void override;

		auto flip(
			std::size_t read,
			std::size_t write
		) -> void override;

		auto mark_updated(
			owner_id_t owner_id
		) -> void;

		auto drain_adds(
		) -> std::vector<owner_id_t>;

		auto drain_updates(
		) -> std::vector<owner_id_t>;

		auto drain_removes(
		) -> std::vector<owner_id_t>;

	private:
		double_buffered_id_mapped_queue<component_type, owner_id_t> m_dbq;
		double_buffered_id_mapped_queue<component_type, owner_id_t>::reader m_reader{ &m_dbq };
		double_buffered_id_mapped_queue<component_type, owner_id_t>::writer m_writer{ &m_dbq };

		std::unordered_map<link_id_t, owner_id_t> m_link_to_owner_map;
		std::uint32_t m_next_link_id = 0;

		std::vector<owner_id_t> m_added;
		std::unordered_set<owner_id_t> m_updated;
		mutable std::mutex m_updated_mutex;
		std::vector<owner_id_t> m_removed;
	};
}

template <gse::is_component T>
template <typename... Args>
auto gse::component_link<T>::add(const owner_id_t owner_id, registry* reg, Args&&... args) -> T* {
	if (auto* existing = try_get_write(owner_id)) {
		return existing;
	}

	const link_id_t new_link_id = generate_id(std::string(typeid(T).name()) + std::to_string(m_next_link_id++));
	m_link_to_owner_map[new_link_id] = owner_id;

	T* new_comp = m_writer.emplace_queued(owner_id, std::forward<Args>(args)...);
	new_comp->on_registry(reg);

	return new_comp;
}

template <gse::is_component T>
auto gse::component_link<T>::activate(const owner_id_t owner_id) -> bool {
	if (m_writer.activate(owner_id)) {
		m_added.push_back(owner_id);
		m_reader.emplace_from_writer(owner_id);
		return true;
	}
	return false;
}

template <gse::is_component T>
auto gse::component_link<T>::remove(const owner_id_t owner_id) -> void {
	m_writer.remove(owner_id);
	m_removed.push_back(owner_id);

	std::erase_if(
		m_link_to_owner_map,
		[&](const auto& pair) {
		return pair.second == owner_id;
	}
	);
}

template <gse::is_component T>
auto gse::component_link<T>::objects_read() -> std::span<const component_type> {
	return m_reader.objects();
}

template <gse::is_component T>
auto gse::component_link<T>::objects_write() -> std::span<component_type> {
	return m_writer.objects();
}

template <gse::is_component T>
auto gse::component_link<T>::try_get_read(const owner_id_t owner_id) -> const component_type* {
	return m_reader.try_get(owner_id);
}

template <gse::is_component T>
auto gse::component_link<T>::try_get_write(const owner_id_t owner_id) -> component_type* {
	if (auto* p = m_writer.try_get(owner_id)) {
		std::lock_guard lock(m_updated_mutex);
		m_updated.insert(owner_id);
		return p;
	}
	return nullptr;
}

template <gse::is_component T>
auto gse::component_link<T>::try_get_by_link_id_read(const link_id_t& link_id) -> const component_type* {
	if (const auto it = m_link_to_owner_map.find(link_id); it != m_link_to_owner_map.end()) {
		return m_reader.try_get(it->second);
	}
	return nullptr;
}

template <gse::is_component T>
auto gse::component_link<T>::try_get_by_link_id_write(const link_id_t& link_id) -> component_type* {
	if (const auto it = m_link_to_owner_map.find(link_id); it != m_link_to_owner_map.end()) {
		return m_writer.try_get(it->second);
	}
	return nullptr;
}

template <gse::is_component T>
auto gse::component_link<T>::bind(const std::size_t read_index, const std::size_t write_index) -> void {
	std::tie(m_reader, m_writer) = m_dbq.bind(read_index, write_index);
}

template <gse::is_component T>
auto gse::component_link<T>::flip(std::size_t read, std::size_t write) -> void {
	m_dbq.flip();
	std::tie(m_reader, m_writer) = m_dbq.bind(read, write);
}

template <gse::is_component T>
auto gse::component_link<T>::mark_updated(const owner_id_t owner_id) -> void {
	std::lock_guard lock(m_updated_mutex);
	m_updated.insert(owner_id);
}

template <gse::is_component T>
auto gse::component_link<T>::drain_adds() -> std::vector<owner_id_t> {
	auto out = std::move(m_added);
	m_added.clear();
	return out;
}

template <gse::is_component T>
auto gse::component_link<T>::drain_updates() -> std::vector<owner_id_t> {
	const std::unordered_set added_set(m_added.begin(), m_added.end());

	std::unordered_set<owner_id_t> snapshot;
	{
		std::lock_guard lock(m_updated_mutex);
		snapshot = std::move(m_updated);
		m_updated.clear();
	}

	std::vector<owner_id_t> out;
	out.reserve(snapshot.size());
	for (const auto& id : snapshot) {
		if (!added_set.contains(id)) {
			out.push_back(id);
		}
	}

	return out;
}

template <gse::is_component T>
auto gse::component_link<T>::drain_removes() -> std::vector<owner_id_t> {
	auto out = std::move(m_removed);
	m_removed.clear();
	return out;
}
