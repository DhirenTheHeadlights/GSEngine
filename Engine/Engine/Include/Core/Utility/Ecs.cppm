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
		explicit hook_base(
			T* owner
		);
		virtual ~hook_base() = default;

		auto owner_id() const -> const id&;
	protected:
		T* m_owner;
	};
}

template <gse::is_identifiable T>
gse::hook_base<T>::hook_base(T* owner) : m_owner(owner) {}

template <gse::is_identifiable T>
auto gse::hook_base<T>::owner_id() const -> const id& {
	return m_owner->id();
}

export namespace gse {
	template <typename T>
	class hook : public hook_base<T> {
	public:
		explicit hook(T* owner) : hook_base<T>(owner) {
		}

		virtual auto initialize() -> void {
		}
		virtual auto update() -> void {
		}
		virtual auto render() -> void {
		}
		virtual auto shutdown() -> void {
		}
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
}

export namespace gse {
	template <typename T>
	class hookable : public identifiable {
	public:
		hookable(const std::string& name, std::initializer_list<std::unique_ptr<hook<T>>> hooks = {});

		auto add_hook(
			std::unique_ptr<hook<T>> hook
		) -> void;

		auto initialize() const -> void;
		auto update() const -> void;
		auto render() const -> void;
		auto shutdown() const -> void;

		auto cycle(
			const std::function<bool()>& condition
		) const -> void;
	private:
		std::vector<std::unique_ptr<hook<T>>> m_hooks;
		friend hook<T>;
	};
}

template <typename T>
gse::hookable<T>::hookable(const std::string& name, std::initializer_list<std::unique_ptr<hook<T>>> hooks) : identifiable(name) {
	for (auto&& h : hooks) m_hooks.push_back(std::move(const_cast<std::unique_ptr<hook<T>>&>(h)));
}

template <typename T>
auto gse::hookable<T>::add_hook(std::unique_ptr<hook<T>> hook) -> void {
	m_hooks.push_back(std::move(hook));
}

template <typename T>
auto gse::hookable<T>::initialize() const -> void {
	for (auto& hook : m_hooks) {
		hook->initialize();
	}
}

template <typename T>
auto gse::hookable<T>::update() const -> void {
	for (auto& hook : m_hooks) {
		hook->update();
	}
}

template <typename T>
auto gse::hookable<T>::render() const -> void {
	for (auto& hook : m_hooks) {
		hook->render();
	}
}

template <typename T>
auto gse::hookable<T>::shutdown() const -> void {
	for (auto& hook : m_hooks) {
		hook->shutdown();
	}
}

template <typename T>
auto gse::hookable<T>::cycle(const std::function<bool()>& condition) const -> void {
	initialize();
	while (condition()) {
		update();
		render();
	}
	shutdown();
}

export namespace gse {
	class registry;

	class component_link_base {
	public:
		virtual ~component_link_base() = default;
		virtual auto activate(const id& id) -> bool = 0;
		virtual auto remove(const id& id) -> void = 0;
	};

	template <is_component T>
	class component_link final : public component_link_base {
	public:
		using owner_id_t = id;
		using link_id_t = id;
		using component_type = T;

		template <typename... Args>
		auto add(
			const owner_id_t& owner_id,
			Args&&... args
		) -> std::pair<link_id_t, T*>;

		auto activate(
			const owner_id_t& owner_id
		) -> bool override;

		auto remove(
			const owner_id_t& owner_id
		) -> void override;

		auto objects() -> std::span<component_type>;

		auto try_get(
			const owner_id_t& owner_id
		) -> component_type*;

		auto try_get_by_link_id(
			const link_id_t& link_id
		) -> component_type*;
	private:
		id_mapped_collection<component_type> m_active_components;
		id_mapped_collection<component_type> m_queued_components;
		std::unordered_map<link_id_t, owner_id_t> m_link_to_owner_map;
		std::uint32_t m_next_link_id = 0;
	};
}

template <gse::is_component T>
template <typename ... Args>
auto gse::component_link<T>::add(const owner_id_t& owner_id, Args&&... args) -> std::pair<link_id_t, T*> {
	gse::assert(!try_get(owner_id), std::format(
		"Attempting to add a component of type {} to owner {} that already has one.",
		typeid(T).name(), owner_id
	));

	const link_id_t new_link_id = gse::generate_id(std::string(typeid(T).name()) + std::to_string(m_next_link_id++));
	m_link_to_owner_map[new_link_id] = owner_id;

	T* new_comp = m_queued_components.add(owner_id, T(owner_id, std::forward<Args>(args)...));
	return { new_link_id, new_comp };
}

template <gse::is_component T>
auto gse::component_link<T>::activate(const owner_id_t& owner_id) -> bool {
	if (auto component = m_queued_components.pop(owner_id)) {
		m_active_components.add(owner_id, std::move(*component));
		return true;
	}
	return false;
}

template <gse::is_component T>
auto gse::component_link<T>::remove(const owner_id_t& owner_id) -> void {
	m_active_components.remove(owner_id);
	m_queued_components.remove(owner_id);

	std::erase_if(
		m_link_to_owner_map,
		[&](const auto& pair) {
			return pair.second == owner_id;
		}
	);
}

template <gse::is_component T>
auto gse::component_link<T>::objects() -> std::span<component_type> {
	return m_active_components.items();
}

template <gse::is_component T>
auto gse::component_link<T>::try_get(const owner_id_t& owner_id) -> component_type* {
	if (auto* component = m_active_components.try_get(owner_id)) {
		return component;
	}
	return m_queued_components.try_get(owner_id);
}

template <gse::is_component T>
auto gse::component_link<T>::try_get_by_link_id(const link_id_t& link_id) -> component_type* {
	if (const auto it = m_link_to_owner_map.find(link_id); it != m_link_to_owner_map.end()) {
		return try_get(it->second);
	}
	return nullptr;
}

export namespace gse {
	class hook_link_base {
	public:
		virtual ~hook_link_base() = default;
		virtual auto activate(const id& id) -> bool = 0;
		virtual auto remove(const id& id) -> void = 0;
		virtual auto initialize_hook(const id& owner_id) -> void = 0;
		virtual auto hooks_as_base() -> std::vector<hook<entity>*> = 0;
	};

	template <is_entity_hook T>
	class hook_link final : public hook_link_base {
	public:
		using owner_id_t = id;
		using link_id_t = id;
		using hook_ptr_type = std::unique_ptr<T>;

		explicit hook_link(
			registry& reg
		);

		template <typename... Args>
		auto add(
			const owner_id_t& owner_id, 
			Args&&... args
		) -> std::pair<link_id_t, T*>;

		auto activate(
			const owner_id_t& owner_id
		) -> bool override;

		auto remove(
			const owner_id_t& owner_id
		) -> void override;

		auto initialize_hook(
			const owner_id_t& owner_id
		) -> void override;

		auto hooks_as_base(
		) -> std::vector<hook<entity>*> override;

		auto try_get(
			const owner_id_t& owner_id
		) -> T*;

		auto try_get_by_link_id(
			const link_id_t& link_id
		) -> T*;
	private:
		id_mapped_collection<hook_ptr_type> m_active_hooks;
		id_mapped_collection<hook_ptr_type> m_queued_hooks;
		std::unordered_map<link_id_t, owner_id_t> m_link_to_owner_map;
		std::uint32_t m_next_link_id = 0;
		registry& m_registry;
	};
}

template <gse::is_entity_hook T>
gse::hook_link<T>::hook_link(registry& reg) : m_registry(reg) {}

template <gse::is_entity_hook T>
template <typename ... Args>
auto gse::hook_link<T>::add(const owner_id_t& owner_id, Args&&... args) -> std::pair<link_id_t, T*> {
	gse::assert(
		!try_get(owner_id), 
		std::format(
	        "Attempting to add a hook of type {} to owner {} that already has one.",
	        typeid(T).name(), owner_id
	    )
	);

	const link_id_t new_link_id = gse::generate_id(std::string(typeid(T).name()) + std::to_string(m_next_link_id++));
	m_link_to_owner_map[new_link_id] = owner_id;

	auto new_hook = std::make_unique<T>(std::forward<Args>(args)...);
	T* raw_ptr = new_hook.get();
	raw_ptr->inject(owner_id, &m_registry);
	m_queued_hooks.add(owner_id, std::move(new_hook));

	return { new_link_id, raw_ptr };
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::activate(const owner_id_t& owner_id) -> bool {
	if (auto hook = m_queued_hooks.pop(owner_id)) {
		m_active_hooks.add(owner_id, std::move(*hook));
		return true;
	}
	return false;
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::remove(const owner_id_t& owner_id) -> void {
	m_active_hooks.remove(owner_id);
	m_queued_hooks.remove(owner_id);

	std::erase_if(
		m_link_to_owner_map,
		[&](const auto& pair) {
			return pair.second == owner_id;
		}
	);
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::initialize_hook(const owner_id_t& owner_id) -> void {
	if (auto* hook_ptr = m_active_hooks.try_get(owner_id)) {
		(*hook_ptr)->initialize();
	}
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::hooks_as_base() -> std::vector<hook<entity>*> {
	std::vector<hook<entity>*> base_hooks;
	auto active_hooks_span = m_active_hooks.items();
	base_hooks.reserve(active_hooks_span.size());
	for (const auto& hook_ptr : active_hooks_span) {
		base_hooks.push_back(hook_ptr.get());
	}
	return base_hooks;
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::try_get(const owner_id_t& owner_id) -> T* {
	if (auto* hook_ptr = m_active_hooks.try_get(owner_id)) {
		return hook_ptr->get();
	}
	if (auto* hook_ptr = m_queued_hooks.try_get(owner_id)) {
		return hook_ptr->get();
	}
	return nullptr;
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::try_get_by_link_id(const link_id_t& link_id) -> T* {
	if (const auto it = m_link_to_owner_map.find(link_id); it != m_link_to_owner_map.end()) {
		return try_get(it->second);
	}
	return nullptr;
}

export namespace gse {
	class registry final : public non_copyable {
	public:
		using uuid = std::uint32_t;
		using deferred_action = std::function<bool(registry&)>;

		registry() = default;

		auto create(
			const std::string& name
		) -> id;

		auto activate(
			const id& id
		) -> void;

		auto remove(
			const id& id
		) -> void;

		auto update() -> void;
		auto render() -> void;

		auto add_deferred_action(
			const id& owner_id,
			deferred_action action
		) -> void;

		template <is_component U, typename... Args>
		auto add_component(
			const id& owner_id,
			Args&&... args
		) -> std::pair<id, U*>;

		template <is_entity_hook U, typename... Args>
		auto add_hook(
			const id& owner_id,
			Args&&... args
		) -> std::pair<id, U*>;

		template <typename U>
		auto remove_link(
			const id& id
		) -> void;

		template <typename U>
		auto linked_objects(
		) -> auto;

		auto all_hooks(
		) -> std::vector<hook<entity>*>;

		template <typename U>
		auto linked_object(
			const id& id
		) -> U&;

		template <typename U>
		auto try_linked_object(
			const id& id
		) -> U*;

		template <typename U>
		auto linked_object_by_link_id(
			const id& link_id
		) -> U&;

		template <typename U>
		auto try_linked_object_by_link_id(
			const id& link_id
		) -> U*;

		auto exists(
			const id& id
		) const -> bool;

		auto active(
			const id& id
		) const -> bool;

		template <typename U>
		static auto any_components(
			std::span<std::reference_wrapper<registry>> registries
		) -> bool;
	private:
		id_mapped_collection<entity> m_active_entities;
		std::unordered_set<id> m_inactive_ids;
		std::vector<std::uint32_t> m_free_indices;

		std::unordered_map<std::type_index, std::unique_ptr<component_link_base>> m_component_links;
		std::unordered_map<std::type_index, std::unique_ptr<hook_link_base>> m_hook_links;

		std::unordered_map<id, std::vector<deferred_action>> m_deferred_actions;
	};
}

auto gse::registry::create(const std::string& name) -> id {
	const auto id = generate_id(name);
	m_inactive_ids.insert(id);
	return id;
}

auto gse::registry::activate(const id& id) -> void {
	assert(m_inactive_ids.contains(id), std::format("Cannot activate entity with id {}: it is not inactive.", id));
	m_inactive_ids.erase(id);

	entity object;
	if (!m_free_indices.empty()) {
		object.index = m_free_indices.back();
		m_free_indices.pop_back();
		if (auto* old_entity = m_active_entities.try_get(id)) {
			old_entity->generation++;
			object.generation = old_entity->generation;
		}
	}
	else {
		object.index = static_cast<std::uint32_t>(m_active_entities.size());
	}
	m_active_entities.add(id, object);

	bool work_was_done;
	do {
		work_was_done = false;

		for (const auto& link : m_component_links | std::views::values) {
			if (link->activate(id)) {
				work_was_done = true;
			}
		}

		std::vector<hook_link_base*> current_hook_links;
		current_hook_links.reserve(m_hook_links.size());
		for (const auto& val : m_hook_links | std::views::values) {
			current_hook_links.push_back(val.get());
		}

		for (auto* link : current_hook_links) {
			if (link->activate(id)) {
				link->initialize_hook(id);
				work_was_done = true;
			}
		}
	} while (work_was_done);

	if (const auto it = m_deferred_actions.find(id); it != m_deferred_actions.end()) {
		auto& actions = it->second;

		std::erase_if(
			actions, 
			[&](auto& action) {
				if (action(*this)) {
					return true;
				}

				std::println("WARNING: Deferred action for entity {} did not complete.", id);
				return false;
			}
		);

		if (actions.empty()) {
			m_deferred_actions.erase(it);
		}
	}
}

auto gse::registry::remove(const id& id) -> void {
	if (active(id)) {
		if (const auto* entity = m_active_entities.try_get(id)) {
			m_free_indices.push_back(entity->index);
		}
		m_active_entities.remove(id);
	}
	else {
		m_inactive_ids.erase(id);
	}

	for (const auto& link : m_component_links | std::views::values) {
		link->remove(id);
	}
	for (const auto& link : m_hook_links | std::views::values) {
		link->remove(id);
	}
}

auto gse::registry::update() -> void {
	for (auto& actions : m_deferred_actions | std::views::values) {
		std::erase_if(
			actions, 
			[this](auto& action) {
				return action(*this);
			}
		);
	}

	std::erase_if(
		m_deferred_actions, 
		[](const auto& pair) {
			return pair.second.empty();
		}
	);
}

auto gse::registry::render() -> void {
}

auto gse::registry::add_deferred_action(const id& owner_id, deferred_action action) -> void {
	m_deferred_actions[owner_id].push_back(std::move(action));
}

template <gse::is_component U, typename... Args>
auto gse::registry::add_component(const id& owner_id, Args&&... args) -> std::pair<id, U*> {
	assert(exists(owner_id), std::format("Cannot add component to entity with id {}: it does not exist.", owner_id));

	const auto type_idx = std::type_index(typeid(U));
	if (!m_component_links.contains(type_idx)) {
		m_component_links[type_idx] = std::make_unique<component_link<U>>();
	}

	auto& lnk = static_cast<component_link<U>&>(*m_component_links.at(type_idx));
	return lnk.add(owner_id, std::forward<Args>(args)...);
}

template <gse::is_entity_hook U, typename... Args>
auto gse::registry::add_hook(const id& owner_id, Args&&... args) -> std::pair<id, U*> {
	assert(exists(owner_id), std::format("Cannot add hook to entity with id {}: it does not exist.", owner_id));

	const auto type_idx = std::type_index(typeid(U));
	if (!m_hook_links.contains(type_idx)) {
		m_hook_links[type_idx] = std::make_unique<hook_link<U>>(*this);
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
	}
	else {
		if (const auto it = m_component_links.find(type_idx); it != m_component_links.end()) {
			it->second->remove(id);
		}
	}
}

template <typename U>
auto gse::registry::linked_objects() -> auto {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);

		if (it == m_hook_links.end()) {
			return std::span<U>{};
		}

		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		return lnk.objects();
	}
	else {
		const auto it = m_component_links.find(type_idx);

		if (it == m_component_links.end()) {
			return std::span<U>{};
		}

		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.objects();
	}
}

auto gse::registry::all_hooks() -> std::vector<hook<entity>*> {
	std::vector<hook<entity>*> collected_hooks;

	for (const auto& link_ptr : m_hook_links | std::views::values) {
		auto hooks_from_link = link_ptr->hooks_as_base();
		collected_hooks.insert(collected_hooks.end(), hooks_from_link.begin(), hooks_from_link.end());
	}

	return collected_hooks;
}

template <typename U>
auto gse::registry::linked_object(const id& id) -> U& {
	U* ptr = try_linked_object<U>(id);
	assert(ptr, std::format(
		"Linked object of type {} with id {} not found. If you expect a component/hook and its not there, you may want to try gse::hook<gse::entity>::configure_when_present to modify an component that is added through another hook", 
		typeid(U).name(), id)
	);
	return *ptr;
}

template <typename U>
auto gse::registry::try_linked_object(const id& id) -> U* {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);
		if (it == m_hook_links.end()) return nullptr;
		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		return lnk.try_get(id);
	}
	else {
		const auto it = m_component_links.find(type_idx);
		if (it == m_component_links.end()) return nullptr;
		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.try_get(id);
	}
}

template <typename U>
auto gse::registry::linked_object_by_link_id(const id& link_id) -> U& {
	U* ptr = try_linked_object_by_link_id<U>(link_id);
	assert(ptr);
	return *ptr;
}

template <typename U>
auto gse::registry::try_linked_object_by_link_id(const id& link_id) -> U* {
	const auto type_idx = std::type_index(typeid(U));

	if constexpr (is_entity_hook<U>) {
		const auto it = m_hook_links.find(type_idx);
		if (it == m_hook_links.end()) return nullptr;
		auto& lnk = static_cast<hook_link<U>&>(*it->second);
		return lnk.try_get_by_link_id(link_id);
	}
	else {
		const auto it = m_component_links.find(type_idx);
		if (it == m_component_links.end()) return nullptr;
		auto& lnk = static_cast<component_link<U>&>(*it->second);
		return lnk.try_get_by_link_id(link_id);
	}
}

auto gse::registry::exists(const id& id) const -> bool {
	return m_active_entities.contains(id) || m_inactive_ids.contains(id);
}

auto gse::registry::active(const id& id) const -> bool {
	return m_active_entities.contains(id);
}

template <typename U>
auto gse::registry::any_components(const std::span<std::reference_wrapper<registry>> registries) -> bool {
	return std::ranges::any_of(
		registries, [](const auto& reg) {
			return !reg.get().template linked_objects<U>().empty();
		}
	);
}

namespace gse {
	template<typename F>
	struct lambda_traits : lambda_traits<decltype(&F::operator())> {};

	template<typename ClassType, typename ReturnType, typename Arg>
	struct lambda_traits<ReturnType(ClassType::*)(Arg) const> {
		using component_type = std::remove_cvref_t<Arg>;
	};
}

export template <>
class gse::hook<gse::entity> : public identifiable_owned {
public:
	virtual ~hook() = default;

	virtual auto initialize() -> void {}
	virtual auto update() -> void {}
	virtual auto render() -> void {}
	virtual auto shutdown() -> void {}

	template <is_component T> requires has_params<T>
	auto add_component(
		T::params p
	) -> std::pair<id, T*>;

	template <is_component T> requires (!has_params<T>)
	auto add_component(
	) -> std::pair<id, T*>;

	template <is_entity_hook T> requires has_params<T>
	auto add_hook(
		T::params p
	) -> std::pair<id, T*>;

	template <is_entity_hook T> requires (!has_params<T>)
	auto add_hook(
	) -> std::pair<id, T*>;

	template <typename T>
	auto remove(
	) const -> void;

	template <typename T>
	auto component(
	) -> T&;

	template <typename T>
	auto try_component(
	) -> T*;

	template <is_component T>
	auto configure_when_present(
		std::function<void(T&)> config_func
	) -> void;

	template <typename Func>
	auto configure_when_present(
		Func&& config_func
	) -> void;
private:
	registry* m_registry = nullptr;

	template <is_entity_hook T>
	friend class hook_link;

	auto inject(
		const id& owner_id, 
		registry* reg
	) -> void;
};

template <gse::is_component T> requires gse::has_params<T>
auto gse::hook<gse::entity>::add_component(typename T::params p) -> std::pair<id, T*> {
	return m_registry->add_component<T>(owner_id(), std::move(p));
}

template <gse::is_component T> requires (!gse::has_params<T>)
auto gse::hook<gse::entity>::add_component() -> std::pair<id, T*> {
	return m_registry->add_component<T>(owner_id());
}

template <gse::is_entity_hook T> requires gse::has_params<T>
auto gse::hook<gse::entity>::add_hook(typename T::params p) -> std::pair<id, T*> {
	return m_registry->add_hook<T>(owner_id(), std::move(p));
}

template <gse::is_entity_hook T> requires (!gse::has_params<T>)
auto gse::hook<gse::entity>::add_hook() -> std::pair<id, T*> {
	return m_registry->add_hook<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::remove() const -> void {
	m_registry->remove_link<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::component() -> T& {
	assert(m_registry->active(owner_id()), std::format("Cannot access component of type {} for entity with id {}: it is not active.", typeid(T).name(), owner_id()));
	return m_registry->linked_object<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::try_component() -> T* {
	assert(m_registry->exists(owner_id()), std::format("Cannot access component of type {} for entity with id {}: it does not exist.", typeid(T).name(), owner_id()));
	return m_registry->try_linked_object<T>(owner_id());
}

template <gse::is_component T>
auto gse::hook<gse::entity>::configure_when_present(std::function<void(T&)> config_func) -> void {
	registry::deferred_action action = [owner_id = this->owner_id(), config_func = std::move(config_func)](registry& reg) -> bool {
		if (auto* component = reg.try_linked_object<T>(owner_id)) {
			config_func(*component);
			return true;
		}
		return false;
		};

	m_registry->add_deferred_action(owner_id(), std::move(action));
}

template <typename Func>
auto gse::hook<gse::entity>::configure_when_present(Func&& config_func) -> void {
	using c = lambda_traits<Func>::component_type;
	configure_when_present<c>(std::forward<Func>(config_func));
}

auto gse::hook<gse::entity>::inject(const id& owner_id, registry* reg) -> void {
	assert(reg != nullptr, std::format("Registry cannot be null for hook with owner id {}.", owner_id));
	m_registry = reg;
	swap(owner_id);
}

export namespace gse {
	class scene final : public hookable<scene> {
	public:
		explicit scene(
			const std::string& name = "Unnamed Scene"
		);

		auto add_entity(
			const std::string& name
		) -> gse::id;

		auto remove_entity(
			const gse::id& id
		) -> void;

		auto set_active(
			bool is_active
		) -> void;

		auto active(
		) const -> bool;

		auto entities(
		) const -> std::span<const gse::id>;

		auto registry(
		) -> registry&;
	private:
		gse::registry m_registry;
		std::vector<gse::id> m_entities;
		std::vector<gse::id> m_queue;

		friend class default_scene;

		bool m_is_active = false;
	};
}

gse::scene::scene(const std::string& name) : hookable(name) {
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
	assert(m_registry.exists(id), std::format("Cannot remove entity with id {}: it does not exist.", id));
	m_registry.remove(id);
	std::erase(m_entities, id);
}

auto gse::scene::set_active(const bool is_active) -> void {
	this->m_is_active = is_active;
}

auto gse::scene::active() const -> bool {
	return m_is_active;
}

auto gse::scene::entities() const -> std::span<const gse::id> {
	return m_entities;
}

auto gse::scene::registry() -> gse::registry& {
	return m_registry;
}

export namespace gse {
	template <>
	class hook<scene> : public identifiable_owned {
	public:
		class builder {
		public:
			builder(const id& entity_id, scene* scene);

			template <typename T>
				requires (is_entity_hook<T> || is_component<T>) && has_params<T>
			auto with(
				const T::params& p
			) -> builder&;

			template <typename T>
				requires (is_entity_hook<T> || is_component<T>) && !has_params<T>
			auto with(
			) -> builder&;
		private:
			id m_entity_id;
			scene* m_scene;
		};

		hook(scene* owner);
		virtual ~hook() = default;

		virtual auto initialize() -> void {}
		virtual auto update() -> void {}
		virtual auto render() -> void {}
		virtual auto shutdown() -> void {}

		auto build(
			const std::string& name
		) const -> builder;
	protected:
		scene* m_owner = nullptr;
	};
}

gse::hook<gse::scene>::builder::builder(const id& entity_id, scene* scene) : m_entity_id(entity_id), m_scene(scene) {}

template <typename T> requires (gse::is_entity_hook<T> || gse::is_component<T>) && gse::has_params<T>
auto gse::hook<gse::scene>::builder::with(const typename T::params& p) -> builder& {
	if constexpr (is_entity_hook<T>) {
		m_scene->registry().add_hook<T>(m_entity_id, p);
	}
	else {
		m_scene->registry().add_component<T>(m_entity_id, p);
	}
	return *this;
}

template <typename T> requires (gse::is_entity_hook<T> || gse::is_component<T>) && !gse::has_params<T>
auto gse::hook<gse::scene>::builder::with() -> builder& {
	if constexpr (is_entity_hook<T>) {
		m_scene->registry().add_hook<T>(m_entity_id);
	}
	else {
		m_scene->registry().add_component<T>(m_entity_id);
	}
	return *this;
}

gse::hook<gse::scene>::hook(scene* owner) : identifiable_owned(owner->id()), m_owner(owner) {
	assert(m_owner != nullptr, std::format("Scene owner cannot be null for hook with owner id {}.", owner->id()));
}

auto gse::hook<gse::scene>::build(const std::string& name) const -> builder {
	const auto id = m_owner->add_entity(name);
	return builder(id, m_owner);
}

export namespace gse {
	class default_scene final : public hook<scene> {
	public:
		explicit default_scene(scene* owner);

		auto initialize() -> void override;
		auto update() -> void override;
		auto render() -> void override;
		auto shutdown() -> void override;
	};
}

gse::default_scene::default_scene(scene* owner) : hook(owner) {}

auto gse::default_scene::initialize() -> void {
	for (const auto& object_id : m_owner->m_queue) {
		m_owner->m_registry.activate(object_id);
		m_owner->m_entities.push_back(object_id);
	}

	m_owner->m_queue.clear();
}

auto gse::default_scene::update() -> void {
	m_owner->registry().update();
	bulk_invoke(m_owner->registry().all_hooks(), &hook<entity>::update);
}

auto gse::default_scene::render() -> void {
	m_owner->registry().render();
	bulk_invoke(m_owner->registry().all_hooks(), &hook<entity>::render);
}

auto gse::default_scene::shutdown() -> void {
	bulk_invoke(m_owner->registry().all_hooks(), &hook<entity>::shutdown);

	for (const auto& id : m_owner->m_entities) {
		m_owner->m_registry.remove(id);
	}
}
