export module gse.utility:registry;

import std;

import :hook;
import :id;
import :non_copyable;

import gse.assert;
import gse.physics.math;

export namespace gse {
	template <typename T>
	concept linkable_object = requires(T obj) {
		{ obj.owner_id() } -> std::convertible_to<id>;
	};

	template <typename T>
	concept is_hook = std::derived_from<T, hook<entity>>;

	class registry final : public non_copyable {
	public:
		using uuid = std::uint32_t;

		class link_base {
		public:
			virtual ~link_base() = default;
			virtual auto add(const id& id) -> void* = 0;
			virtual auto activate(const id& id) -> void = 0;
			virtual auto remove(const id& id) -> void = 0;
		};

		template <linkable_object T>
		class link final : public link_base {
		public:
			auto add(const id& id) -> void* override {
				m_id_to_index_map[id] = m_queued_objects.size();

				if constexpr (is_hook<T>) {
					m_queued_objects.emplace_back(std::make_unique<T>(id));
				}
				else {
					m_queued_objects.emplace_back(id);
				}

				return pointer_to_object(m_queued_objects.back());
			}

			auto activate(const id& id) -> void override {
				const auto map_it = m_id_to_index_map.find(id);
				if (map_it == m_id_to_index_map.end()) {
					return;
				}

				size_t index_in_queue = map_it->second;

				assert(index_in_queue < m_queued_objects.size(), "Index out of bounds");
				assert(owner_id(m_queued_objects[index_in_queue]) == id, "ID mismatch");

				m_linked_objects.push_back(std::move(m_queued_objects[index_in_queue]));

				std::swap(m_queued_objects[index_in_queue], m_queued_objects.back());
				m_queued_objects.pop_back();

				if (index_in_queue < m_queued_objects.size()) {
					const auto swapped_id = owner_id(m_queued_objects[index_in_queue]);
					m_id_to_index_map[swapped_id] = index_in_queue;
				}

				m_id_to_index_map[id] = m_linked_objects.size() - 1;
			}

			auto remove(const id& id) -> void override {
				const auto map_it = m_id_to_index_map.find(id);
				if (map_it == m_id_to_index_map.end()) {
					return;
				}

				const size_t index_to_remove = map_it->second;
				const size_t last_index = m_linked_objects.size() - 1;

				if (m_linked_objects.empty()) {
					m_id_to_index_map.erase(map_it);
					return;
				}

				const auto last_element_owner_id = owner_id(m_linked_objects.back());

				std::swap(m_linked_objects[index_to_remove], m_linked_objects.back());
				m_linked_objects.pop_back();
				m_id_to_index_map.erase(map_it);

				if (index_to_remove != last_index) {
					m_id_to_index_map[last_element_owner_id] = index_to_remove;
				}
			}
		private:
			using container_type = std::conditional_t<
				is_hook<T>,
				std::vector<std::unique_ptr<T>>,
				std::vector<T>
			>;

			static auto owner_id(const auto& element) -> const id& {
				if constexpr (is_hook<T>) {
					return element->owner_id(); 
				}
				else {
					return element.owner_id(); 
				}
			}

			static auto pointer_to_object(auto& element) -> void* {
				if constexpr (is_hook<T>) {
					return element.get();
				}
				else {
					return &element;
				}
			}

			container_type m_linked_objects;
			container_type m_queued_objects;
			std::unordered_map<id, std::size_t> m_id_to_index_map;
		};

		registry() = default;

		auto create(const std::string& name) -> id;
		auto activate(const id& id) -> void;
		auto remove(const id& id) -> void;

		template <linkable_object U> requires !std::derived_from<U, hook<entity>>
		auto add_link(const id& id) -> U*;

		template <linkable_object U>
		auto remove_link(const id& id) -> void;

		template <linkable_object U>
		auto linked_objects() -> std::span<U>;

		template <linkable_object U>
		auto linked_object(const id& id) -> U&;

		auto exists(const id& id) const -> bool;
		auto active(const id& id) const -> bool;
	private:
		std::vector<entity> m_active_objects;
		std::vector<entity> m_inactive_objects;
		std::vector<std::uint32_t> m_free_indices;
		std::unordered_map<id, uuid> m_entity_map;
		std::unordered_map<std::type_index, std::unique_ptr<link_base>> m_links;
	};
}

auto gse::registry::create(const std::string& name) -> id {
	m_inactive_objects.emplace_back();
	const auto id = generate_id(name);
	m_entity_map[id] = static_cast<uuid>(m_inactive_objects.size() - 1);
	return id;
}

auto gse::registry::activate(const id& id) -> void {
	const auto inactive_index = m_entity_map[id];

	const auto it = std::ranges::find_if(
		m_inactive_objects,
		[inactive_index](const entity& obj) {
			return obj.index == inactive_index;
		}
	);

	assert(it != m_inactive_objects.end(), "Object not found in registry when trying to activate it");

	auto object = *it;
	std::erase(m_inactive_objects, *it);

	for (const auto& link : m_links | std::views::values) {
		link->activate(id);
	}

	std::uint32_t index;
	if (!m_free_indices.empty()) {
		index = m_free_indices.back();
		m_free_indices.pop_back();

		m_active_objects[index].generation++;

		object.generation = m_active_objects[index].generation;
		object.index = index;

		m_active_objects[index] = object;
	}
	else {
		index = static_cast<std::uint32_t>(m_active_objects.size());
		object.index = index;
		object.generation = 0;
		m_active_objects.push_back(object);
	}

	m_entity_map[id] = index;
}

auto gse::registry::remove(const id& id) -> void {
	const auto index = m_entity_map.at(id);
	m_free_indices.push_back(index);
	m_active_objects[index].generation++;

	m_entity_map.erase(id);

	for (const auto& link : m_links | std::views::values) {
		link->remove(id);
	}
}

template <gse::linkable_object U> requires !std::derived_from<U, gse::hook<gse::entity>>
auto gse::registry::add_link(const id& id) -> U* {
	assert(
		exists(id),
		std::format("Object with id {} does not exist in registry when trying to add a link", id)
	);

	if (!m_links.contains(typeid(U))) {
		m_links[typeid(U)] = std::make_unique<link<U>>();
	}

	auto& link = static_cast<link<U>&>(*m_links.at(typeid(U)));
	return static_cast<void*>(link.add(id));
}

template <gse::linkable_object U>
auto gse::registry::remove_link(const id& id) -> void {
	if (const auto it = m_links.find(typeid(U)); it != m_links.end()) {
		it->second->remove(id);
	}
	else {
		assert(false, "Link type not found in registry.");
	}
}

template <gse::linkable_object U>
auto gse::registry::linked_objects() -> std::span<U> {
	if (const auto it = m_links.find(typeid(U)); it != m_links.end()) {
		auto& link = static_cast<link<U>&>(*it->second);
		return link.m_linked_objects;
	}
	return {};
}

template <gse::linkable_object U>
auto gse::registry::linked_object(const id& id) -> U& {
	const auto it = m_links.find(typeid(U));
	if (it == m_links.end()) {
		throw std::runtime_error("Link type not found in registry.");
	}

	const auto& link = static_cast<link<U>&>(*it->second);
	const auto it2 = link.m_id_to_index_map.find(id);
	if (it2 == link.m_id_to_index_map.end()) {
		throw std::runtime_error(std::format("Object with id {} doesn't have a link of this type.", id));
	}

	return link.m_linked_objects[it2->second];
}

auto gse::registry::exists(const id& id) const -> bool {
	return m_entity_map.contains(id);
}

auto gse::registry::active(const id& id) const -> bool {
	return m_entity_map.contains(id) && m_active_objects[m_entity_map.at(id)].generation % 2 == 0;
}