export module gse.utility:ecs;

import std;

import :id;
import :component;
import :non_copyable;
import :misc;

export namespace gse {
	template <is_identifiable T>
	class hook_base {
	public:
		virtual ~hook_base() = default;
		explicit hook_base(T* owner) : m_owner(owner) {}

		auto owner_id() -> const id& {
			return m_owner->id();
		}
	protected:
		T* m_owner;
	};

	template <typename T>
	class hook : public hook_base<T> {
	public:
		explicit hook(T* owner) : hook_base<T>(owner) {}

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
		virtual auto shutdown() -> void {}
	};

	struct entity {
		std::uint32_t index = random_value(std::numeric_limits<std::uint32_t>::max());
		std::uint32_t generation = 0;

		auto operator==(const entity& other) const -> bool = default;
	};

	template <typename T>
	concept is_entity_hook = std::derived_from<T, hook<entity>>;

	template <typename T>
	concept is_component = !is_entity_hook<T>;

	template <typename T>
	concept has_params = requires { typename T::params; };

	template <typename T>
	class hookable : public identifiable {
	public:
		hookable(const std::string& name, std::initializer_list<std::unique_ptr<hook<T>>> hooks = {}) : identifiable(name) {
			for (auto&& h : hooks) m_hooks.push_back(std::move(const_cast<std::unique_ptr<hook<T>>&>(h)));
		}

		auto add_hook(std::unique_ptr<hook<T>> hook) -> void {
			m_hooks.push_back(std::move(hook));
		}

		auto initialize() const -> void {
			for (auto& hook : m_hooks) {
				hook->initialize();
			}
		}

		auto update() const -> void {
			for (auto& hook : m_hooks) {
				hook->update();
			}
		}

		auto render() const -> void {
			for (auto& hook : m_hooks) {
				hook->render();
			}
		}

		auto shutdown() const -> void {
			for (auto& hook : m_hooks) {
				hook->shutdown();
			}
		}

		auto loop_while(const std::function<bool()>& condition) const -> void {
			while (condition()) {
				update();
				render();
			}
		}
	private:
		std::vector<std::unique_ptr<hook<T>>> m_hooks;
		friend hook<T>;
	};
}

export namespace gse {
	class registry;

	class component_link_base {
	public:
		virtual ~component_link_base() = default;
		virtual auto activate(const id& id) -> void = 0;
		virtual auto remove(const id& id) -> void = 0;
	};

	template <is_component T>
	class component_link final : public component_link_base {
		friend registry;
	public:
		using owner_id_t = id;
		using link_id_t = id;
		using component_type = T;

		template <typename... Args>
		auto add(const owner_id_t& owner_id, Args&&... args) -> std::pair<link_id_t, T*> {
			const link_id_t new_link_id = generate_id(std::string(typeid(T).name()) + std::to_string(m_next_link_id++));
			const size_t new_index = m_queued_components.size();

			m_queued_owner_ids.push_back(owner_id);
			m_queued_link_ids.push_back(new_link_id);

			m_id_to_index_map[owner_id] = new_index;
			m_link_id_to_index_map[new_link_id] = new_index;

			auto& new_comp = m_queued_components.emplace_back(owner_id, std::forward<Args>(args)...);
			return { new_link_id, &new_comp };
		}

		auto activate(const owner_id_t& owner_id) -> void override {
			const auto map_it = m_id_to_index_map.find(owner_id);
			if (map_it == m_id_to_index_map.end()) {
				return;
			}

			const size_t index_in_queue = map_it->second;

			if (index_in_queue >= m_queued_components.size() || m_queued_owner_ids[index_in_queue] != owner_id) {
				return;
			}

			m_linked_components.push_back(std::move(m_queued_components[index_in_queue]));
			m_linked_link_ids.push_back(m_queued_link_ids[index_in_queue]);
			m_linked_owner_ids.push_back(m_queued_owner_ids[index_in_queue]);
			const size_t new_index_in_linked = m_linked_components.size() - 1;

			if (const size_t last_queued_index = m_queued_components.size() - 1; index_in_queue != last_queued_index) {
				m_queued_components[index_in_queue] = std::move(m_queued_components.back());
				m_queued_link_ids[index_in_queue] = m_queued_link_ids.back();
				m_queued_owner_ids[index_in_queue] = m_queued_owner_ids.back();

				const auto& swapped_owner_id = m_queued_owner_ids[index_in_queue];
				const auto& swapped_link_id = m_queued_link_ids[index_in_queue];
				m_id_to_index_map[swapped_owner_id] = index_in_queue;
				m_link_id_to_index_map[swapped_link_id] = index_in_queue;
			}

			m_queued_components.pop_back();
			m_queued_link_ids.pop_back();
			m_queued_owner_ids.pop_back();

			const auto& activated_link_id = m_linked_link_ids[new_index_in_linked];
			m_id_to_index_map[owner_id] = new_index_in_linked;
			m_link_id_to_index_map[activated_link_id] = new_index_in_linked;
		}

		auto remove(const owner_id_t& owner_id) -> void override {
			const auto map_it = m_id_to_index_map.find(owner_id);
			if (map_it == m_id_to_index_map.end()) {
				return;
			}

			const size_t index_to_remove = map_it->second;

			if (index_to_remove >= m_linked_components.size() || m_linked_owner_ids[index_to_remove] != owner_id) {
				return;
			}

			const auto link_id_to_remove = m_linked_link_ids[index_to_remove];

			if (const size_t last_linked_index = m_linked_components.size() - 1; index_to_remove != last_linked_index) {
				m_linked_components[index_to_remove] = std::move(m_linked_components.back());
				m_linked_link_ids[index_to_remove] = m_linked_link_ids.back();
				m_linked_owner_ids[index_to_remove] = m_linked_owner_ids.back();

				const auto& swapped_owner_id = m_linked_owner_ids[index_to_remove];
				const auto& swapped_link_id = m_linked_link_ids[index_to_remove];
				m_id_to_index_map[swapped_owner_id] = index_to_remove;
				m_link_id_to_index_map[swapped_link_id] = index_to_remove;
			}

			m_linked_components.pop_back();
			m_linked_link_ids.pop_back();
			m_linked_owner_ids.pop_back();

			m_id_to_index_map.erase(owner_id);
			m_link_id_to_index_map.erase(link_id_to_remove);
		}

		auto objects() -> auto& {
			return m_linked_components;
		}

	private:
		std::vector<component_type> m_linked_components;
		std::vector<link_id_t> m_linked_link_ids;
		std::vector<owner_id_t> m_linked_owner_ids;

		std::vector<component_type> m_queued_components;
		std::vector<link_id_t> m_queued_link_ids;
		std::vector<owner_id_t> m_queued_owner_ids;

		std::unordered_map<owner_id_t, std::size_t> m_id_to_index_map;
		std::unordered_map<link_id_t, std::size_t> m_link_id_to_index_map;
		std::uint32_t m_next_link_id = 0;
	};

	class hook_link_base {
	public:
		virtual ~hook_link_base() = default;
		virtual auto activate(const id& id) -> void = 0;
		virtual auto remove(const id& id) -> void = 0;
		virtual auto initialize_hook(const id& owner_id) -> void = 0;
	};

	template <is_entity_hook T>
	class hook_link final : public hook_link_base {
		friend class registry;
	public:
		using owner_id_t = id;
		using link_id_t = id;
		using component_type = std::unique_ptr<T>;

		template <typename... Args>
		auto add(const owner_id_t& owner_id, Args&&... args) -> std::pair<link_id_t, T*> {
			const link_id_t new_link_id = generate_id(std::string(typeid(T).name()) + std::to_string(m_next_link_id++));
			const size_t new_index = m_queued_components.size();

			m_queued_owner_ids.push_back(owner_id);
			m_queued_link_ids.push_back(new_link_id);

			m_id_to_index_map[owner_id] = new_index;
			m_link_id_to_index_map[new_link_id] = new_index;

			auto& new_comp = m_queued_components.emplace_back(std::make_unique<T>(owner_id, std::forward<Args>(args)...));
			return { new_link_id, new_comp.get() };
		}

		auto activate(const owner_id_t& owner_id) -> void override {
			const auto map_it = m_id_to_index_map.find(owner_id);
			if (map_it == m_id_to_index_map.end()) {
				return;
			}

			const size_t index_in_queue = map_it->second;

			if (index_in_queue >= m_queued_components.size() || m_queued_owner_ids[index_in_queue] != owner_id) {
				return;
			}

			m_linked_components.push_back(std::move(m_queued_components[index_in_queue]));
			m_linked_link_ids.push_back(m_queued_link_ids[index_in_queue]);
			m_linked_owner_ids.push_back(m_queued_owner_ids[index_in_queue]);
			const size_t new_index_in_linked = m_linked_components.size() - 1;

			if (const size_t last_queued_index = m_queued_components.size() - 1; index_in_queue != last_queued_index) {
				m_queued_components[index_in_queue] = std::move(m_queued_components.back());
				m_queued_link_ids[index_in_queue] = m_queued_link_ids.back();
				m_queued_owner_ids[index_in_queue] = m_queued_owner_ids.back();

				const auto& swapped_owner_id = m_queued_owner_ids[index_in_queue];
				const auto& swapped_link_id = m_queued_link_ids[index_in_queue];
				m_id_to_index_map[swapped_owner_id] = index_in_queue;
				m_link_id_to_index_map[swapped_link_id] = index_in_queue;
			}

			m_queued_components.pop_back();
			m_queued_link_ids.pop_back();
			m_queued_owner_ids.pop_back();

			const auto& activated_link_id = m_linked_link_ids[new_index_in_linked];
			m_id_to_index_map[owner_id] = new_index_in_linked;
			m_link_id_to_index_map[activated_link_id] = new_index_in_linked;
		}

		auto remove(const owner_id_t& owner_id) -> void override {
			const auto map_it = m_id_to_index_map.find(owner_id);
			if (map_it == m_id_to_index_map.end()) {
				return;
			}

			const size_t index_to_remove = map_it->second;

			if (index_to_remove >= m_linked_components.size() || m_linked_owner_ids[index_to_remove] != owner_id) {
				return;
			}

			const auto link_id_to_remove = m_linked_link_ids[index_to_remove];

			if (const size_t last_linked_index = m_linked_components.size() - 1; index_to_remove != last_linked_index) {
				m_linked_components[index_to_remove] = std::move(m_linked_components.back());
				m_linked_link_ids[index_to_remove] = m_linked_link_ids.back();
				m_linked_owner_ids[index_to_remove] = m_linked_owner_ids.back();

				const auto& swapped_owner_id = m_linked_owner_ids[index_to_remove];
				const auto& swapped_link_id = m_linked_link_ids[index_to_remove];
				m_id_to_index_map[swapped_owner_id] = index_to_remove;
				m_link_id_to_index_map[swapped_link_id] = index_to_remove;
			}

			m_linked_components.pop_back();
			m_linked_link_ids.pop_back();
			m_linked_owner_ids.pop_back();

			m_id_to_index_map.erase(owner_id);
			m_link_id_to_index_map.erase(link_id_to_remove);
		}

		auto initialize_hook(const owner_id_t& owner_id) -> void override {
			const auto map_it = m_id_to_index_map.find(owner_id);
			if (map_it == m_id_to_index_map.end()) {
				return;
			}

			const size_t index_in_linked = map_it->second;
			if (index_in_linked >= m_linked_components.size() || m_linked_owner_ids[index_in_linked] != owner_id) {
				return;
			}

			if (auto& hook_ptr = m_linked_components[index_in_linked]) {
				hook_ptr->initialize();
			}
		}

		auto objects() -> auto& {
			return m_linked_components;
		}

	private:
		std::vector<component_type> m_linked_components;
		std::vector<link_id_t> m_linked_link_ids;
		std::vector<owner_id_t> m_linked_owner_ids;

		std::vector<component_type> m_queued_components;
		std::vector<link_id_t> m_queued_link_ids;
		std::vector<owner_id_t> m_queued_owner_ids;

		std::unordered_map<owner_id_t, std::size_t> m_id_to_index_map;
		std::unordered_map<link_id_t, std::size_t> m_link_id_to_index_map;
		std::uint32_t m_next_link_id = 0;
	};

	class registry final : public non_copyable {
	public:
		using uuid = std::uint32_t;

		registry() = default;

		auto create(const std::string& name) -> id;
		auto activate(const id& id) -> void;
		auto remove(const id& id) -> void;

		template <is_component U, typename... Args>
		auto add_component(const id& owner_id, Args&&... args) -> std::pair<id, U*>;

		template <is_entity_hook U, typename... Args>
		auto add_hook(const id& owner_id, Args&&... args) -> std::pair<id, U*>;

		template <typename U>
		auto remove_link(const id& id) -> void;

		template <typename U>
		auto linked_objects() -> auto&;

		template <typename U>
		auto linked_object(const id& id) -> U&;

		template <typename U>
		auto try_linked_object(const id& id) -> U*;

		template <typename U>
		auto linked_object_by_link_id(const id& link_id) -> U&;

		template <typename U>
		auto try_linked_object_by_link_id(const id& link_id) -> U*;

		auto exists(const id& id) const -> bool;
		auto active(const id& id) const -> bool;

		template <typename U>
		static auto any_components(std::span<std::reference_wrapper<registry>> registries) -> bool;
	private:
		std::vector<entity> m_active_objects;
		std::vector<entity> m_inactive_objects;
		std::vector<std::uint32_t> m_free_indices;
		std::unordered_map<id, uuid> m_entity_map;

		std::unordered_map<std::type_index, std::unique_ptr<component_link_base>> m_component_links;
		std::unordered_map<std::type_index, std::unique_ptr<hook_link_base>> m_hook_links;
	};
}

auto gse::registry::create(const std::string& name) -> id {
	m_inactive_objects.emplace_back();
	const auto id = generate_id(name);
	m_entity_map[id] = static_cast<uuid>(m_inactive_objects.size() - 1);
	return id;
}

auto gse::registry::activate(const id& id) -> void {
	const auto it_map = m_entity_map.find(id);
	if (it_map == m_entity_map.end()) {
		assert(false, "Entity ID not found for activation.");
		return;
	}
	const auto inactive_index = it_map->second;

	const auto it = std::ranges::find_if(
		m_inactive_objects,
		[inactive_index](const entity& obj) {
			return obj.index == inactive_index;
		}
	);

	assert(it != m_inactive_objects.end(), "Object not found in registry when trying to activate it");

	auto object = *it;
	std::erase(m_inactive_objects, *it);

	for (const auto& link : m_component_links | std::views::values) {
		link->activate(id);
	}

	for (const auto& link : m_hook_links | std::views::values) {
		link->activate(id);
	}

	for (const auto& link : m_hook_links | std::views::values) {
		link->initialize_hook(id);
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

	for (const auto& link : m_component_links | std::views::values) {
		link->remove(id);
	}
	for (const auto& link : m_hook_links | std::views::values) {
		link->remove(id);
	}
}

template <gse::is_component U, typename... Args>
auto gse::registry::add_component(const id& owner_id, Args&&... args) -> std::pair<id, U*> {
	assert(exists(owner_id), "Object does not exist in registry");

	const auto type_idx = std::type_index(typeid(U));
	if (!m_component_links.contains(type_idx)) {
		m_component_links[type_idx] = std::make_unique<component_link<U>>();
	}

	auto& lnk = static_cast<component_link<U>&>(*m_component_links.at(type_idx));
	return lnk.add(owner_id, std::forward<Args>(args)...);
}

template <gse::is_entity_hook U, typename... Args>
auto gse::registry::add_hook(const id& owner_id, Args&&... args) -> std::pair<id, U*> {
	assert(exists(owner_id), "Object does not exist in registry");

	const auto type_idx = std::type_index(typeid(U));
	if (!m_hook_links.contains(type_idx)) {
		m_hook_links[type_idx] = std::make_unique<hook_link<U>>();
	}

	auto& lnk = static_cast<hook_link<U>&>(*m_hook_links.at(type_idx));
	return lnk.add(owner_id, std::forward<Args>(args)...);
}

template <typename U>
auto gse::registry::remove_link(const id& id) -> void {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		if (const auto it = m_hook_links.find(type_idx); it != m_hook_links.end()) {
			it->second->remove(id);
		}
		else {
			assert(false, "Hook link type not found in registry.");
		}
	}
	else {
		if (const auto it = m_component_links.find(type_idx); it != m_component_links.end()) {
			it->second->remove(id);
		}
		else {
			assert(false, "Component link type not found in registry.");
		}
	}
}

template <typename U>
auto gse::registry::linked_objects() -> auto& {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);
		assert(it != m_hook_links.end(), std::format("Hook link type {} not found in registry.", typeid(U).name()));
		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		return lnk.objects();
	}
	else { // is_component
		const auto it = m_component_links.find(type_idx);
		assert(it != m_component_links.end(), std::format("Component link type {} not found in registry.", typeid(U).name()));
		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.objects();
	}
}

template <typename U>
auto gse::registry::linked_object(const id& id) -> U& {
	U* ptr = try_linked_object<U>(id);
	if (!ptr) {
		throw std::runtime_error(std::format("Object with id {} doesn't have a linked object of type {}.", id, typeid(U).name()));
	}
	return *ptr;
}

template <typename U>
auto gse::registry::try_linked_object(const id& id) -> U* {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);
		if (it == m_hook_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		const auto map_it = lnk.m_id_to_index_map.find(id);
		if (map_it == lnk.m_id_to_index_map.end() || map_it->second >= lnk.m_linked_components.size()) {
			return nullptr;
		}
		return lnk.m_linked_components[map_it->second].get();
	}
	else {
		const auto it = m_component_links.find(type_idx);
		if (it == m_component_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<component_link<U>&>(*it->second);
		const auto map_it = lnk.m_id_to_index_map.find(id);
		if (map_it == lnk.m_id_to_index_map.end() || map_it->second >= lnk.m_linked_components.size()) {
			return nullptr;
		}
		return &lnk.m_linked_components[map_it->second];
	}
}

template <typename U>
auto gse::registry::linked_object_by_link_id(const id& link_id) -> U& {
	U* ptr = try_linked_object_by_link_id<U>(link_id);
	if (!ptr) {
		throw std::runtime_error(std::format("Link with id {} of type {} does not exist.", link_id, typeid(U).name()));
	}
	return *ptr;
}

template <typename U>
auto gse::registry::try_linked_object_by_link_id(const id& link_id) -> U* {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);
		if (it == m_hook_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		const auto map_it = lnk.m_link_id_to_index_map.find(link_id);
		if (map_it == lnk.m_link_id_to_index_map.end() || map_it->second >= lnk.m_linked_components.size()) {
			return nullptr;
		}
		return lnk.m_linked_components[map_it->second].get();
	}
	else {
		const auto it = m_component_links.find(type_idx);
		if (it == m_component_links.end()) {
			return nullptr;
		}
		auto& lnk = static_cast<component_link<U>&>(*it->second);
		const auto map_it = lnk.m_link_id_to_index_map.find(link_id);
		if (map_it == lnk.m_link_id_to_index_map.end() || map_it->second >= lnk.m_linked_components.size()) {
			return nullptr;
		}
		return &lnk.m_linked_components[map_it->second];
	}
}

auto gse::registry::exists(const id& id) const -> bool {
	return m_entity_map.contains(id);
}

auto gse::registry::active(const id& id) const -> bool {
	if (!m_entity_map.contains(id)) {
		return false;
	}
	const auto& [index, generation] = m_active_objects[m_entity_map.at(id)];
	return generation % 2 == 0;
}

template <typename U>
auto gse::registry::any_components(const std::span<std::reference_wrapper<registry>> registries) -> bool {
	return std::ranges::any_of(
		registries,
		[](const std::reference_wrapper<registry>& reg) {
			return !reg.get().template linked_objects<U>().empty();
		}
	);
}

export template <>
class gse::hook<gse::entity> : public identifiable_owned {
public:
	virtual ~hook() = default;
	explicit hook(const id& owner_id, registry* reg);

	virtual auto initialize() -> void {}
	virtual auto update() -> void {}
	virtual auto render() -> void {}

	template <is_component T> requires has_params<T>
	auto add_component(typename T::params p) -> std::pair<id, T*>;

	template <is_component T> requires (!has_params<T>)
		auto add_component() -> std::pair<id, T*>;

	template <is_entity_hook T> requires has_params<T>
	auto add_hook(typename T::params p) -> std::pair<id, T*>;

	template <is_entity_hook T> requires (!has_params<T>)
		auto add_hook() -> std::pair<id, T*>;

	template <typename T>
	auto remove() const -> void;

	template <typename T>
	auto component() -> T&;

	template <typename T>
	auto try_component() -> T*;
private:
	registry* m_registry = nullptr;
};

gse::hook<gse::entity>::hook(const id& owner_id, registry* reg) : identifiable_owned(owner_id), m_registry(reg) {
	assert(m_registry != nullptr, std::format("Scene for hook '{}' cannot be null.", owner_id.tag()));
}

template <gse::is_component T> requires gse::has_params<T>
auto gse::hook<gse::entity>::add_component(typename T::params p) -> std::pair<id, T*> {
	return m_registry->add_component<T>(owner_id(), std::forward<const typename T::params&>(p));
}

template <gse::is_component T> requires (!gse::has_params<T>)
auto gse::hook<gse::entity>::add_component() -> std::pair<id, T*> {
	return m_registry->add_component<T>(owner_id());
}

template <gse::is_entity_hook T> requires gse::has_params<T>
auto gse::hook<gse::entity>::add_hook(const typename T::params p) -> std::pair<id, T*> {
	return m_registry->add_hook<T>(owner_id(), m_registry, std::forward<const typename T::params&>(p));
}

template <gse::is_entity_hook T> requires (!gse::has_params<T>)
auto gse::hook<gse::entity>::add_hook() -> std::pair<id, T*> {
	return m_registry->add_hook<T>(owner_id(), m_registry);
}

template <typename T>
auto gse::hook<gse::entity>::remove() const -> void {
	m_registry->remove_link<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::component() -> T& {
	return m_registry->linked_object<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::try_component() -> T* {
	return m_registry->try_linked_object<T>(owner_id());
}

export namespace gse {
	class scene final : public hookable<scene> {
	public:
		explicit scene(const std::string& name = "Unnamed Scene");

		auto add_entity(const std::string& name) -> gse::id;
		auto remove_entity(const gse::id& id) -> void;

		auto set_active(const bool is_active) -> void { this->m_is_active = is_active; }
		auto active() const -> bool { return m_is_active; }
		auto entities() const -> std::span<const gse::id>;
		auto registry() -> registry& { return m_registry; }
	private:
		gse::registry m_registry;
		std::vector<gse::id> m_entities;
		std::vector<gse::id> m_queue;

		bool m_is_active = false;
	};

	template <>
	class hook<scene> : public identifiable_owned {
	public:
		class builder {
		public:
			builder(const id& entity_id, scene* scene) : m_entity_id(entity_id), m_scene(scene) {}

			template <typename T>
				requires (is_entity_hook<T> || is_component<T>) && has_params<T>
			auto with(const typename T::params& p) -> builder& {
				if constexpr (is_entity_hook<T>) {
					m_scene->registry().add_hook<T>(m_entity_id, &m_scene->registry(), p);
				}
				else {
					m_scene->registry().add_component<T>(m_entity_id, p);
				}
				return *this;
			}

			template <typename T>
				requires (is_entity_hook<T> || is_component<T>) && !has_params<T>
			auto with() -> builder& {
				if constexpr (is_entity_hook<T>) {
					m_scene->registry().add_hook<T>(m_entity_id, &m_scene->registry());
				}
				else {
					m_scene->registry().add_component<T>(m_entity_id);
				}
				return *this;
			}
		private:
			id m_entity_id;
			scene* m_scene;
		};

		hook(scene* owner) : identifiable_owned(owner->id()), m_owner(owner) {
			assert(m_owner != nullptr, std::format("Scene for hook '{}' cannot be null.", owner->id().tag()));
		}
		virtual ~hook() = default;

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
		virtual auto shutdown() -> void {}

		auto build(const std::string& name) const -> builder {
			const auto id = m_owner->add_entity(name);
			return builder(id, m_owner);
		}
	protected:
		scene* m_owner = nullptr;
	};
}

gse::scene::scene(const std::string& name) : hookable(name) {
	class default_scene final : public hook<scene> {
	public:
		explicit default_scene(scene* owner) : hook(owner) {}

		auto initialize() -> void override {
			for (const auto& object_id : m_owner->m_queue) {
				m_owner->m_registry.activate(object_id);
				m_owner->m_entities.push_back(object_id);
			}

			m_owner->m_queue.clear();

			bulk_invoke(m_owner->m_registry.linked_objects<hook<entity>>(), &hook<entity>::initialize);
		}

		auto update() -> void override {
			bulk_invoke(m_owner->m_registry.linked_objects<hook<entity>>(), &hook<entity>::update);
		}

		auto render() -> void override {
			bulk_invoke(m_owner->m_registry.linked_objects<hook<entity>>(), &hook<entity>::render);
		}
	};

	add_hook(std::make_unique<default_scene>(this));
}

auto gse::scene::add_entity(const std::string& name) -> gse::id {
	const auto id = m_registry.create(name);

	if (m_is_active) {
		m_registry.activate(id);
		m_entities.push_back(id);
	}
	else {
		m_queue.push_back(id);
	}

	return id;
}

auto gse::scene::remove_entity(const gse::id& id) -> void {
	assert(m_registry.exists(id), std::format("Entity '{}' not found in scene '{}'", id, this->id().tag()));
	m_registry.remove(id);
	std::erase(m_entities, id);
}

auto gse::scene::entities() const -> std::span<const gse::id> {
	return m_entities;
}

