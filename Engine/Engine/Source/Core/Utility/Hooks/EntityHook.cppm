export module gse.utility:entity_hook;

import std;

import :id;
import :misc;
import :lambda_traits;

import :concepts;
import :entity;
import :registry;
import :hook_link;

export template <>
class gse::hook<gse::entity> : public identifiable_owned {
public:
	virtual ~hook() = default;

	virtual auto initialize(
	) -> void {}

	virtual auto update(
	) -> void {}

	virtual auto render(
	) -> void {}

	virtual auto shutdown(
	) -> void {}

	template <is_component T> requires has_params<T>
	auto add_component(
		T::params p
	) -> T*;

	template <is_component T> requires (!has_params<T>)
		auto add_component(
		) -> T*;

	template <is_entity_hook T> requires has_params<T>
	auto add_hook(
		T::params p
	) -> T*;

	template <is_entity_hook T> requires (!has_params<T>)
	auto add_hook(
	) -> T*;

	template <typename T>
	auto remove(
	) const -> void;

	template <typename T>
	auto component_read(
	) -> const T&;

	template <typename T>
	auto component_write(
	) -> T&;

	template <typename T>
	auto try_component_read(
	) -> const T*;

	template <typename T>
	auto try_component_write(
	) -> T*;

	template <is_component T>
	auto configure_when_present(
		const std::function<void(T&)>& config_func
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
		id owner_id,
		registry* reg
	) -> void;
};

template <gse::is_component T> requires gse::has_params<T>
auto gse::hook<gse::entity>::add_component(typename T::params p) -> T* {
	return m_registry->add_component<T>(owner_id(), std::move(p));
}

template <gse::is_component T> requires (!gse::has_params<T>)
auto gse::hook<gse::entity>::add_component() -> T* {
	return m_registry->add_component<T>(owner_id());
}

template <gse::is_entity_hook T> requires gse::has_params<T>
auto gse::hook<gse::entity>::add_hook(typename T::params p) -> T* {
	return m_registry->add_hook<T>(owner_id(), std::move(p));
}

template <gse::is_entity_hook T> requires (!gse::has_params<T>)
auto gse::hook<gse::entity>::add_hook() -> T* {
	return m_registry->add_hook<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::remove() const -> void {
	m_registry->remove_link<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::component_read() -> const T& {
	assert(m_registry->active(owner_id()), std::source_location::current(), "Cannot access component of type {} for entity with id {}: it is not active.", typeid(T).name(), owner_id());
	return m_registry->linked_object_read<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::component_write() -> T& {
	assert(m_registry->active(owner_id()), std::source_location::current(), "Cannot access component of type {} for entity with id {}: it is not active.", typeid(T).name(), owner_id());
	return m_registry->linked_object_write<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::try_component_read() -> const T* {
	assert(m_registry->active(owner_id()), std::source_location::current(), "Cannot access component of type {} for entity with id {}: it is not active.", typeid(T).name(), owner_id());
	return m_registry->try_linked_object_read<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::try_component_write() -> T* {
	assert(m_registry->active(owner_id()), std::source_location::current(), "Cannot access component of type {} for entity with id {}: it is not active.", typeid(T).name(), owner_id());
	return m_registry->try_linked_object_write<T>(owner_id());
}

template <gse::is_component T>
auto gse::hook<gse::entity>::configure_when_present(const std::function<void(T&)>& config_func) -> void {
	registry::deferred_action action = [owner_id = this->owner_id(), config_func](registry& reg) -> bool {
		if (auto* component = reg.try_linked_object_write<T>(owner_id)) {
			config_func(*component);
			return true;
		}
		return false;
	};

	m_registry->add_deferred_action(owner_id(), std::move(action));
}

template <typename Func>
auto gse::hook<gse::entity>::configure_when_present(Func&& config_func) -> void {
	using c = first_arg_t<Func>;
	configure_when_present<c>(std::forward<Func>(config_func));
}

auto gse::hook<gse::entity>::inject(id owner_id, registry* reg) -> void {
	assert(reg != nullptr, std::source_location::current(), "Registry cannot be null for hook with owner id {}.", owner_id);
	m_registry = reg;
	swap_parent(owner_id);
}
