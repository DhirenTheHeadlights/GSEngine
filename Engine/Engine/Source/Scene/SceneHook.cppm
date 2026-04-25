module;

#include <meta>

export module gse.scene:scene_hook;

import std;

import gse.core;
import gse.ecs;
import gse.hooks;

export namespace gse {
	class scene;

	class scene_init_context {
	public:
		scene_init_context(
			id entity_id,
			registry* reg
		);

		auto id(
		) const -> gse::id;

		template <is_component T> requires has_params<T>
		auto add_component(
			const T::params& p
		) -> T*;

		template <is_component T> requires (!has_params<T>)
		auto add_component(
		) -> T*;

		template <typename T>
		auto remove(
		) -> void;

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
	private:
		gse::id m_entity_id;
		registry* m_registry = nullptr;
	};
}

template <>
class gse::hook<gse::scene> : public identifiable_owned {
public:
	class builder {
	public:
		using init_fn = gse::move_only_function<void(id, registry&)>;

		builder(
			id entity_id,
			scene* owner,
			registry* reg
		);

		template <is_component T> requires has_params<T>
		auto with(
			const T::params& p
		) -> builder&;

		template <is_component T> requires (!has_params<T>)
		auto with(
		) -> builder&;

		auto initialize(
			gse::move_only_function<void(scene_init_context&)> fn
		) -> builder&;

		template <typename Func>
		auto configure(
			Func&& fn
		) -> builder&;

		auto identify(
		) const -> id;
	private:
		auto push_init(
			init_fn fn
		) -> void;

		id m_entity_id;
		scene* m_scene = nullptr;
		registry* m_registry = nullptr;
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

gse::scene_init_context::scene_init_context(const gse::id entity_id, registry* reg) : m_entity_id(entity_id), m_registry(reg) {}

auto gse::scene_init_context::id() const -> gse::id {
	return m_entity_id;
}

template <gse::is_component T> requires gse::has_params<T>
auto gse::scene_init_context::add_component(const typename T::params& p) -> T* {
	return m_registry->add_component<T>(m_entity_id, p);
}

template <gse::is_component T> requires (!gse::has_params<T>)
auto gse::scene_init_context::add_component() -> T* {
	return m_registry->add_component<T>(m_entity_id);
}

template <typename T>
auto gse::scene_init_context::remove() -> void {
	m_registry->remove_component<T>(m_entity_id);
}

template <typename T>
auto gse::scene_init_context::component_read() -> const T& {
	return m_registry->component<T>(m_entity_id);
}

template <typename T>
auto gse::scene_init_context::component_write() -> T& {
	return m_registry->component<T>(m_entity_id);
}

template <typename T>
auto gse::scene_init_context::try_component_read() -> const T* {
	return m_registry->try_component<T>(m_entity_id);
}

template <typename T>
auto gse::scene_init_context::try_component_write() -> T* {
	return m_registry->try_component<T>(m_entity_id);
}

auto gse::hook<gse::scene>::initialize() -> void {}
auto gse::hook<gse::scene>::update() -> void {}
auto gse::hook<gse::scene>::render() -> void {}
auto gse::hook<gse::scene>::shutdown() -> void {}

gse::hook<gse::scene>::builder::builder(const id entity_id, scene* owner, registry* reg) : m_entity_id(entity_id), m_scene(owner), m_registry(reg) {}

template <gse::is_component T> requires gse::has_params<T>
auto gse::hook<gse::scene>::builder::with(const typename T::params& p) -> builder& {
	m_registry->add_component<T>(m_entity_id, p);
	return *this;
}

template <gse::is_component T> requires (!gse::has_params<T>)
auto gse::hook<gse::scene>::builder::with() -> builder& {
	m_registry->add_component<T>(m_entity_id);
	return *this;
}

template <typename Func>
auto gse::hook<gse::scene>::builder::configure(Func&& fn) -> builder& {
	using c = std::remove_cvref_t<typename [: std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<Func>::operator())[0]) :]>;
	push_init([fn = std::forward<Func>(fn)](const id self, registry& reg) mutable {
		if (auto* component = reg.try_component<c>(self)) {
			fn(*component);
		}
	});
	return *this;
}

auto gse::hook<gse::scene>::builder::identify() const -> id {
	return m_entity_id;
}
