export module gse.utility:hook_link;

import std;

import :id;
import :component;
import :misc;
import :entity;
import :hook;
import :concepts;

export namespace gse {
	class hook_link_base {
	public:
		virtual ~hook_link_base() = default;
		virtual auto activate(id id) -> bool = 0;
		virtual auto remove(id id) -> void = 0;
		virtual auto initialize_hook(id owner_id) -> void = 0;
		virtual auto hooks_as_base() -> std::vector<hook<entity>*> = 0;
	};
}

export namespace gse {
	class registry;

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
			owner_id_t owner_id,
			Args&&... args
		) -> T*;

		auto activate(
			owner_id_t owner_id
		) -> bool override;

		auto remove(
			owner_id_t owner_id
		) -> void override;

		auto initialize_hook(
			owner_id_t owner_id
		) -> void override;

		auto hooks_as_base(
		) -> std::vector<hook<entity>*> override;

		auto try_get(
			owner_id_t owner_id
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
gse::hook_link<T>::hook_link(registry& reg) : m_registry(reg) {
}

template <gse::is_entity_hook T>
template <typename ... Args>
auto gse::hook_link<T>::add(const owner_id_t owner_id, Args&&... args) -> T* {
	gse::assert(
		!try_get(owner_id),
		std::source_location::current(),
		"Attempting to add a hook of type {} to owner {} that already has one.",
		typeid(T).name(), owner_id
	);

	const link_id_t new_link_id = generate_id(std::string(typeid(T).name()) + std::to_string(m_next_link_id++));
	m_link_to_owner_map[new_link_id] = owner_id;

	auto new_hook = std::make_unique<T>(std::forward<Args>(args)...);
	T* raw_ptr = new_hook.get();
	raw_ptr->inject(owner_id, &m_registry);
	m_queued_hooks.add(owner_id, std::move(new_hook));

	return raw_ptr;
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::activate(const owner_id_t owner_id) -> bool {
	if (auto hook = m_queued_hooks.pop(owner_id)) {
		m_active_hooks.add(owner_id, std::move(*hook));
		return true;
	}
	return false;
}

template <gse::is_entity_hook T>
auto gse::hook_link<T>::remove(const owner_id_t owner_id) -> void {
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
auto gse::hook_link<T>::initialize_hook(const owner_id_t owner_id) -> void {
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
auto gse::hook_link<T>::try_get(const owner_id_t owner_id) -> T* {
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