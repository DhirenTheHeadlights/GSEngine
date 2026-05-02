export module gse.scene:scene;

import std;
import gse.std_meta;

import gse.assert;
import gse.core;
import gse.ecs;

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

	class scene final : public identifiable {
	public:
		using player_factory_fn = std::function<gse::id(scene&, std::optional<gse::id>)>;
		using init_fn = gse::move_only_function<void(gse::id, registry&)>;
		using setup_fn = void(*)(scene&);

		class builder {
		public:
			builder(
				gse::id entity_id,
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
			) const -> gse::id;
		private:
			auto push_init(
				init_fn fn
			) -> void;

			gse::id m_entity_id;
			scene* m_scene = nullptr;
			registry* m_registry = nullptr;
		};

		explicit scene(
			registry& registry,
			std::string_view name = "Unnamed Scene"
		);

		auto add_entity(
			const std::string& name
		) -> gse::id;

		auto remove_entity(
			const gse::id& id
		) -> void;

		auto build(
			const std::string& name
		) -> builder;

		auto set_setup(
			setup_fn setup
		) -> void;

		auto set_active(
			bool is_active
		) -> void;

		auto active(
		) const -> bool;

		auto entities(
		) const -> std::span<const gse::id>;

		auto registry(
		) const -> registry&;

		auto set_player_factory(
			player_factory_fn factory
		) -> void;

		auto player_factory(
		) const -> const player_factory_fn&;

		auto push_init(
			gse::id entity_id,
			init_fn fn
		) -> void;
	private:
		gse::registry& m_registry;
		std::vector<gse::id> m_entities;
		std::vector<gse::id> m_queue;
		std::vector<std::pair<gse::id, init_fn>> m_pending_inits;
		player_factory_fn m_player_factory;
		setup_fn m_setup;

		bool m_is_active = false;
	};
}

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

gse::scene::scene(gse::registry& registry, const std::string_view name) : identifiable(std::string(name)), m_registry(registry) {}

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
	assert(m_registry.exists(id), std::source_location::current(), "Cannot remove entity with id {}: it does not exist.", id);

	m_registry.remove(id);
	std::erase(m_entities, id);
}

auto gse::scene::build(const std::string& name) -> builder {
	const auto id = add_entity(name);
	return builder(id, this, &m_registry);
}

auto gse::scene::set_setup(setup_fn setup) -> void {
	m_setup = std::move(setup);
}

auto gse::scene::set_active(const bool is_active) -> void {
	if (is_active && !m_is_active) {
		if (m_setup) {
			m_setup(*this);
		}

		for (const auto& object_id : m_queue) {
			m_registry.activate(object_id);
			m_entities.push_back(object_id);
		}
		m_queue.clear();

		for (auto& [id, fn] : m_pending_inits) {
			fn(id, m_registry);
		}
		m_pending_inits.clear();
	}
	else if (!is_active && m_is_active) {
		for (const auto& id : m_entities) {
			m_registry.remove(id);
		}
	}

	m_is_active = is_active;
}

auto gse::scene::active() const -> bool {
	return m_is_active;
}

auto gse::scene::entities() const -> std::span<const gse::id> {
	return m_entities;
}

auto gse::scene::registry() const -> gse::registry& {
	return m_registry;
}

auto gse::scene::set_player_factory(player_factory_fn factory) -> void {
	m_player_factory = std::move(factory);
}

auto gse::scene::player_factory() const -> const player_factory_fn& {
	return m_player_factory;
}

auto gse::scene::push_init(const gse::id entity_id, init_fn fn) -> void {
	m_pending_inits.emplace_back(entity_id, std::move(fn));
}

gse::scene::builder::builder(const gse::id entity_id, scene* owner, gse::registry* reg) : m_entity_id(entity_id), m_scene(owner), m_registry(reg) {}

template <gse::is_component T> requires gse::has_params<T>
auto gse::scene::builder::with(const typename T::params& p) -> builder& {
	m_registry->add_component<T>(m_entity_id, p);
	return *this;
}

template <gse::is_component T> requires (!gse::has_params<T>)
auto gse::scene::builder::with() -> builder& {
	m_registry->add_component<T>(m_entity_id);
	return *this;
}

auto gse::scene::builder::push_init(init_fn fn) -> void {
	m_scene->push_init(m_entity_id, std::move(fn));
}

auto gse::scene::builder::initialize(gse::move_only_function<void(scene_init_context&)> fn) -> builder& {
	push_init([fn = std::move(fn)](const gse::id self, gse::registry& reg) mutable {
		scene_init_context ctx(self, &reg);
		fn(ctx);
	});
	return *this;
}

template <typename Func>
auto gse::scene::builder::configure(Func&& fn) -> builder& {
	using c = std::remove_cvref_t<typename [: std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<Func>::operator())[0]) :]>;
	push_init([fn = std::forward<Func>(fn)](const gse::id self, gse::registry& reg) mutable {
		if (auto* component = reg.try_component<c>(self)) {
			fn(*component);
		}
	});
	return *this;
}

auto gse::scene::builder::identify() const -> gse::id {
	return m_entity_id;
}
