export module gse.runtime:registry;

import std;

import gse.assert;
import gse.utility;
import gse.physics.math;
import gse.platform;

export namespace gse {
	struct entity {
		std::uint32_t index = random_value(std::numeric_limits<std::uint32_t>::max());
		std::uint32_t generation = 0;
	};

	template <>
	struct hook<entity> {
		virtual ~hook() = default;
		hook() = default;

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
	};

	template <typename T>
	concept linkable_object = requires(T obj) {
		{ obj.owner_id } -> std::convertible_to<std::uint32_t>;
	};

	class registry final : public non_copyable {
	public:
		using uuid = std::uint32_t;

		class link_base {
		public:
			virtual ~link_base() = default;
			virtual auto activate(uuid id) -> void = 0;
			virtual auto remove(uuid id) -> void = 0;
			virtual auto update_id(uuid pre, uuid post) -> void = 0;
		};

		template <linkable_object T>
		class link final : public link_base {
		public:
			auto activate(const uuid id) -> void override {
				const auto it = std::ranges::find_if(
					m_queued_objects,
					[id](const T& obj) {
						return obj.owner_id == id;
					}
				);

				if (it == m_queued_objects.end()) {
					return;
				}
				
				m_linked_objects.push_back(*it);
				m_id_to_index_map.insert({ id, m_linked_objects.size() - 1 });

				m_queued_objects.erase(it);
			}

			auto remove(const uuid id) -> void override {
				const auto it = m_id_to_index_map.find(id);

				if (it == m_id_to_index_map.end()) {
					return;
				}

				const size_t index_to_remove = it->second;
				const size_t last_index = m_linked_objects.size() - 1;
				const uuid last_element_owner_id = m_linked_objects.back().owner_id;

				std::swap(m_linked_objects[index_to_remove], m_linked_objects.back());

				m_linked_objects.pop_back();
				m_id_to_index_map.erase(it);

				if (index_to_remove != last_index) {
					m_id_to_index_map[last_element_owner_id] = index_to_remove;
				}
			}

			auto update_id(const uuid pre, const uuid post) -> void override {
				const auto it = m_id_to_index_map.find(pre);

				if (it == m_id_to_index_map.end()) {
					return;
				}

				m_linked_objects[it->second].owner_id = post;
				auto node = m_id_to_index_map.extract(it);

				node.key() = post;
				m_id_to_index_map.insert(std::move(node));
			}
		private:
			std::vector<T> m_linked_objects;
			std::vector<T> m_queued_objects;
			std::unordered_map<uuid, std::size_t> m_id_to_index_map;
		};

		registry() = default;

		auto create() -> uuid;
		auto activate(uuid id, const std::string& name) -> uuid;
		auto remove(uuid id) -> void;

		template <linkable_object U>
		auto add_link(uuid id) -> U*;

		template <linkable_object U>
		auto remove_link(uuid id) -> void;

		template <linkable_object U>
		auto linked_objects() -> std::span<U>;

		template <linkable_object U>
		auto linked_object(uuid id) -> U&;

		auto exists(uuid id) const -> bool;
		auto active(uuid id) const -> bool;
		auto find(std::string_view name) const -> std::optional<uuid>;
	private:
		std::vector<entity> m_active_objects;
		std::vector<entity> m_inactive_objects;
		std::vector<std::uint32_t> m_free_indices;
		std::unordered_map<std::string, uuid> m_string_to_index_map;
		std::unordered_map<std::type_index, std::unique_ptr<link_base>> m_links;
	};
}

auto gse::registry::create() -> uuid {
	m_inactive_objects.emplace_back();
	return m_inactive_objects.back().index;
}

auto gse::registry::activate(const uuid id, const std::string& name) -> uuid {
	const auto it = std::ranges::find_if(
		m_inactive_objects,
		[id](const entity& obj) {
			return obj.index == id;
		}
	);

	assert(it != m_inactive_objects.end(), "Object not found in registry when trying to activate it");

	auto object = *it;
	std::erase(m_inactive_objects, *it);

	for (const auto& link : m_links | std::views::values) {
		link->activate(object.index);
	}

	std::uint32_t index;
	const std::uint32_t previous_id = object.index;
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

	if (m_string_to_index_map.contains(name)) {
		m_string_to_index_map[name + std::to_string(index)] = index;
	}
	else {
		m_string_to_index_map[name] = index;
	}

	for (const auto& link : m_links | std::views::values) {
		link->update_id(previous_id, index);
	}

	return index;
}

auto gse::registry::remove(const uuid id) -> void {
	assert(id < m_active_objects.size(), std::format("Object with id {} does not exist in registry when trying to remove it", id));

	m_free_indices.push_back(id);
	m_active_objects[id].generation++;

	std::erase_if(
		m_string_to_index_map, 
		[id](const auto& pair) {
			return pair.second == id;
		}
	);

	for (const auto& link : m_links | std::views::values) {
		link->remove(id);
	}
}

template <gse::linkable_object U> 
auto gse::registry::add_link(uuid id) -> U* {
	assert(
		exists(id),
		std::format("Object with id {} does not exist in registry when trying to add a link", id)
	);

	if (!m_links.contains(typeid(U))) {
		m_links[typeid(U)] = std::make_unique<link<U>>();
	}

	auto& link = static_cast<link<U>&>(*m_links.at(typeid(U)));

	if (active(id)) {
		U& obj = link.m_linked_objects.emplace_back();
		obj.owner_id = id;
		link.m_id_to_index_map[id] = link.m_linked_objects.size() - 1;
		return &obj;
	}

	U& obj = link.m_queued_objects.emplace_back();
	obj.owner_id = id;
	return &obj;
}

template <gse::linkable_object U>
auto gse::registry::remove_link(const uuid id) -> void {
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
auto gse::registry::linked_object(uuid id) -> U& {
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

auto gse::registry::exists(const uuid id) const -> bool {
	return active(id) || std::ranges::find_if(m_inactive_objects, [id](const entity& obj) {
		return obj.index == id;
		}) != m_inactive_objects.end();
}

auto gse::registry::active(const uuid id) const -> bool {
	return id < m_active_objects.size() && m_active_objects[id].generation % 2 == 0;
}

auto gse::registry::find(const std::string_view name) const -> std::optional<uuid> {
	if (const auto it = m_string_to_index_map.find(std::string(name)); it != m_string_to_index_map.end()) {
		return it->second;
	}
	return std::nullopt;
}