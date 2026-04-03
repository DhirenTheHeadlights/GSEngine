export module gse.utility:scene_hook;

import std;

import :id;
import :misc;

import :hook;
import :registry;
import :concepts;
import :behavior_hook;

export namespace gse {
	class scene;
}

export template <>
class gse::hook<gse::scene> : public identifiable_owned {
public:
	class builder {
	public:
		builder(
			id entity_id,
			registry* reg
		);

		template <typename T>
			requires (is_entity_hook<T> || is_component<T>) && has_params<T>
		auto with(
			const T::params& p
		) -> builder&;

		template <typename T>
			requires (is_entity_hook<T> || is_component<T>) && !has_params<T>
		auto with(
		) -> builder&;

		auto with_init(
			behavior_hook::behavior_func func
		) -> builder&;

		auto with_update(
			behavior_hook::behavior_func func
		) -> builder&;

		auto with_render(
			behavior_hook::behavior_func func
		) -> builder&;

		auto with_shutdown(
			behavior_hook::behavior_func func
		) -> builder&;

		auto identify(
		) const -> id;
	private:
		id m_entity_id;
		registry* m_registry = nullptr;

		auto ensure_behavior_hook(
		) const -> behavior_hook*;
	};

	hook(
		scene* owner
	);

	virtual ~hook(
	) = default;

	virtual auto initialize(
	) -> void;

	virtual auto update(
	) -> void;

	virtual auto render(
	) -> void;

	virtual auto shutdown(
	) -> void;

	auto build(
		const std::string& name
	) const -> builder;
protected:
	scene* m_owner = nullptr;
};

auto gse::hook<gse::scene>::initialize() -> void {}
auto gse::hook<gse::scene>::update() -> void {}
auto gse::hook<gse::scene>::render() -> void {}
auto gse::hook<gse::scene>::shutdown() -> void {}

gse::hook<gse::scene>::builder::builder(const id entity_id, registry* reg) : m_entity_id(entity_id), m_registry(reg) {}

template <typename T>
	requires (gse::is_entity_hook<T> || gse::is_component<T>) && gse::has_params<T>
auto gse::hook<gse::scene>::builder::with(const typename T::params& p) -> builder& {
	if constexpr (is_entity_hook<T>) {
		m_registry->add_hook<T>(m_entity_id, p);
	}
	else {
		m_registry->add_component<T>(m_entity_id, p);
	}
	return *this;
}

template <typename T>
	requires (gse::is_entity_hook<T> || gse::is_component<T>) && !gse::has_params<T>
auto gse::hook<gse::scene>::builder::with() -> builder& {
	if constexpr (is_entity_hook<T>) {
		m_registry->add_hook<T>(m_entity_id);
	}
	else {
		m_registry->add_component<T>(m_entity_id);
	}
	return *this;
}

auto gse::hook<gse::scene>::builder::with_init(behavior_hook::behavior_func func) -> builder& {
	ensure_behavior_hook()->on_init(std::move(func));
	return *this;
}

auto gse::hook<gse::scene>::builder::with_update(behavior_hook::behavior_func func) -> builder& {
	ensure_behavior_hook()->on_update(std::move(func));
	return *this;
}

auto gse::hook<gse::scene>::builder::with_render(behavior_hook::behavior_func func) -> builder& {
	ensure_behavior_hook()->on_render(std::move(func));
	return *this;
}

auto gse::hook<gse::scene>::builder::with_shutdown(behavior_hook::behavior_func func) -> builder& {
	ensure_behavior_hook()->on_shutdown(std::move(func));
	return *this;
}

auto gse::hook<gse::scene>::builder::identify() const -> id {
	return m_entity_id;
}

auto gse::hook<gse::scene>::builder::ensure_behavior_hook() const -> behavior_hook* {
	if (auto* hook = m_registry->try_linked_object_write<behavior_hook>(m_entity_id)) {
		return hook;
	}
	return m_registry->add_hook<behavior_hook>(m_entity_id);
}
