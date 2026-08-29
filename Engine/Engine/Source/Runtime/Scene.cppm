export module gse.runtime:scene;

import std;

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

		auto id() const -> id;

		template <typename T>
		auto add_component(
			T value = T{}
		) -> T*;

		template <typename T>
		auto remove() -> void;

		template <typename T>
		auto component_read() -> const T&;

		template <typename T>
		auto component_write() -> T&;

		template <typename T>
		auto try_component_read() -> const T*;

		template <typename T>
		auto try_component_write() -> T*;

	private:
		gse::id m_entity_id;
		registry* m_registry = nullptr;
	};

	class scene final : public identifiable {
	public:
		class mutation_scope : public non_copyable {
		public:
			mutation_scope(
				scene& target,
				context& ctx
			);

			~mutation_scope();

		private:
			scene* m_scene = nullptr;
		};

		using init_fn = std::move_only_function<void(gse::id, registry&)>;
		using setup_fn = void (
				*
		)(
			scene&
		);

		class builder {
		public:
			builder(
				gse::id entity_id,
				scene* owner,
				registry* reg
			);

			template <typename T>
			auto with(
				T value = T{}
			) -> builder&;

			auto initialize(
				std::move_only_function<void(scene_init_context&)> fn
			) -> builder&;

			template <typename Func>
			auto configure(
				Func&& fn
			) -> builder&;

			auto identify() const -> gse::id;

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

		auto adopt_entity(
			gse::id entity_id
		) -> gse::id;

		auto remove_entity(
			const gse::id& id
		) -> void;

		auto build(
			const std::string& name
		) -> builder;

		auto build(
			gse::id entity_id
		) -> builder;

		template <typename Archetype>
		auto spawn(
			const std::string& name,
			Archetype&& archetype
		) -> gse::id;

		auto set_setup(
			setup_fn setup
		) -> void;

		auto set_active(
			bool is_active
		) -> void;

		auto active() const -> bool;

		auto entities() const -> std::span<const gse::id>;

		auto registry() const -> registry&;

		auto push_init(
			gse::id entity_id,
			init_fn fn
		) -> void;

		[[nodiscard]] auto mutation_context() const -> context*;

	private:
		gse::registry& m_registry;
		context* m_ctx = nullptr;
		std::vector<gse::id> m_entities;
		std::vector<gse::id> m_queue;
		std::vector<std::pair<gse::id, init_fn>> m_pending_inits;
		setup_fn m_setup;

		bool m_is_active = false;
	};
}

gse::scene_init_context::scene_init_context(const gse::id entity_id, registry* reg)
	: m_entity_id(entity_id), m_registry(reg) {
}

auto gse::scene_init_context::id() const -> gse::id {
	return m_entity_id;
}

template <typename T>
auto gse::scene_init_context::add_component(T value) -> T* {
	return m_registry->add_component<T>(m_entity_id, std::move(value));
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

gse::scene::scene(gse::registry& registry, const std::string_view name)
	: identifiable(std::string(name)), m_registry(registry) {
}

gse::scene::mutation_scope::mutation_scope(scene& target, context& ctx) : m_scene(&target) {
	m_scene->m_ctx = &ctx;
}

gse::scene::mutation_scope::~mutation_scope() {
	m_scene->m_ctx = nullptr;
}

auto gse::scene::mutation_context() const -> context* {
	return m_ctx;
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

auto gse::scene::adopt_entity(const gse::id entity_id) -> gse::id {
	auto& owned = m_is_active ? m_entities : m_queue;
	if (std::ranges::find(owned, entity_id) != owned.end()) {
		return entity_id;
	}

	if (m_is_active) {
		m_registry.ensure_active(entity_id);
	}
	owned.push_back(entity_id);

	return entity_id;
}

auto gse::scene::remove_entity(const gse::id& id) -> void {
	assert(m_registry.exists(id), "Cannot remove entity with id {}: it does not exist.", id);

	m_registry.remove(id);
	std::erase(m_entities, id);
}

auto gse::scene::build(const std::string& name) -> builder {
	const auto id = add_entity(name);
	return builder(id, this, &m_registry);
}

auto gse::scene::build(const gse::id entity_id) -> builder {
	return builder(adopt_entity(entity_id), this, &m_registry);
}

template <typename Archetype>
auto gse::scene::spawn(const std::string& name, Archetype&& archetype) -> gse::id {
	const auto id = add_entity(name);
	using arch_t = std::remove_cvref_t<Archetype>;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^arch_t, std::meta::access_context::unchecked()))) {
		using component_t = typename[:std::meta::type_of(m):];
		if (m_ctx != nullptr) {
			m_ctx->add_component<component_t>(id, std::forward_like<Archetype>(archetype.[:m:]));
		}
		else {
			m_registry.add_component<component_t>(id, std::forward_like<Archetype>(archetype.[:m:]));
		}
	}
	return id;
}

auto gse::scene::set_setup(setup_fn setup) -> void {
	m_setup = setup;
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
		m_entities.clear();
		m_queue.clear();
		m_pending_inits.clear();
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

auto gse::scene::push_init(const gse::id entity_id, init_fn fn) -> void {
	m_pending_inits.emplace_back(entity_id, std::move(fn));
}

gse::scene::builder::builder(const gse::id entity_id, scene* owner, gse::registry* reg)
	: m_entity_id(entity_id), m_scene(owner), m_registry(reg) {
}

template <typename T>
auto gse::scene::builder::with(T value) -> builder& {
	if (context* ctx = m_scene->mutation_context()) {
		ctx->add_component<T>(m_entity_id, std::move(value));
		return *this;
	}

	m_registry->add_component<T>(m_entity_id, std::move(value));
	return *this;
}

auto gse::scene::builder::push_init(init_fn fn) -> void {
	m_scene->push_init(m_entity_id, std::move(fn));
}

auto gse::scene::builder::initialize(std::move_only_function<void(scene_init_context&)> fn) -> builder& {
	push_init([fn = std::move(fn)](const gse::id self, gse::registry& reg) mutable {
		scene_init_context ctx(self, &reg);
		fn(ctx);
	});
	return *this;
}

template <typename Func>
auto gse::scene::builder::configure(Func&& fn) -> builder& {
	using c = std::remove_cvref_t<typename[:std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<Func>::operator())[0]):]>;
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