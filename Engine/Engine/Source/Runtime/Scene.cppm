export module gse.runtime:scene;

import std;

import gse.assert;
import gse.core;
import gse.ecs;

export namespace gse {
	class scene final : public identifiable {
	public:
		class mutation_scope : public non_copyable {
		public:
			explicit mutation_scope(
				context& ctx
			);

			~mutation_scope();

		private:
			context* m_ctx = nullptr;
		};

		using setup_fn = std::function<void(scene&)>;

		class builder {
		public:
			builder(
				gse::id entity_id,
				registry& reg
			);

			template <typename T>
			auto with(
				T value = T{}
			) -> builder&;

			auto identify() const -> gse::id;

		private:
			gse::id m_entity_id;
			registry& m_registry;
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

	private:
		[[nodiscard]] static auto mutation_context() -> context*;

		auto add_entity_now(
			const std::string& name
		) -> gse::id;

		auto adopt_entity_now(
			gse::id entity_id
		) -> void;

		auto remove_entity_now(
			gse::id id
		) -> void;

		gse::registry& m_registry;
		std::vector<gse::id> m_entities;
		std::vector<gse::id> m_queue;
		setup_fn m_setup;

		bool m_is_active = false;
	};
}

namespace gse {
	inline thread_local context* tl_mutation_context = nullptr;
}

gse::scene::scene(gse::registry& registry, const std::string_view name)
	: identifiable(std::string(name)), m_registry(registry) {
}

gse::scene::mutation_scope::mutation_scope(context& ctx) : m_ctx(&ctx) {
	assert(tl_mutation_context == nullptr, "another scene::mutation_scope is still open on this thread; a run() held one across a co_await, so close it before awaiting");
	tl_mutation_context = m_ctx;
}

gse::scene::mutation_scope::~mutation_scope() {
	assert(tl_mutation_context == m_ctx, "scene::mutation_scope closed on a thread it was not opened on; it was held across a co_await, so close it before awaiting");
	tl_mutation_context = nullptr;
}

auto gse::scene::mutation_context() -> context* {
	return tl_mutation_context;
}

auto gse::scene::add_entity(const std::string& name) -> gse::id {
	if (context* ctx = mutation_context()) {
		ctx->defer_structural_change([this, name] {
			add_entity_now(name);
		});
		return generate_id(name);
	}
	return add_entity_now(name);
}

auto gse::scene::adopt_entity(const gse::id entity_id) -> gse::id {
	if (context* ctx = mutation_context()) {
		ctx->defer_structural_change([this, entity_id] {
			adopt_entity_now(entity_id);
		});
		return entity_id;
	}
	adopt_entity_now(entity_id);
	return entity_id;
}

auto gse::scene::remove_entity(const gse::id& id) -> void {
	if (context* ctx = mutation_context()) {
		ctx->defer_structural_change([this, id] {
			remove_entity_now(id);
		});
		return;
	}
	remove_entity_now(id);
}

auto gse::scene::add_entity_now(const std::string& name) -> gse::id {
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

auto gse::scene::adopt_entity_now(const gse::id entity_id) -> void {
	auto& owned = m_is_active ? m_entities : m_queue;
	if (std::ranges::find(owned, entity_id) != owned.end()) {
		return;
	}

	if (m_is_active) {
		m_registry.ensure_active(entity_id);
	}
	owned.push_back(entity_id);
}

auto gse::scene::remove_entity_now(const gse::id id) -> void {
	assert(m_registry.exists(id), "Cannot remove entity with id {}: it does not exist.", id);

	m_registry.remove(id);
	std::erase(m_entities, id);
}

auto gse::scene::build(const std::string& name) -> builder {
	return builder(add_entity(name), m_registry);
}

auto gse::scene::build(const gse::id entity_id) -> builder {
	return builder(adopt_entity(entity_id), m_registry);
}

template <typename Archetype>
auto gse::scene::spawn(const std::string& name, Archetype&& archetype) -> gse::id {
	const auto id = add_entity(name);
	context* const ctx = mutation_context();
	using arch_t = std::remove_cvref_t<Archetype>;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^arch_t, std::meta::access_context::unchecked()))) {
		using component_t = typename[:std::meta::type_of(m):];
		if (ctx != nullptr) {
			ctx->add_component<component_t>(id, std::forward_like<Archetype>(archetype.[:m:]));
		}
		else {
			m_registry.add_component<component_t>(id, std::forward_like<Archetype>(archetype.[:m:]));
		}
	}
	return id;
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
	}
	else if (!is_active && m_is_active) {
		for (const auto& id : m_entities) {
			m_registry.remove(id);
		}
		m_entities.clear();
		m_queue.clear();
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

gse::scene::builder::builder(const gse::id entity_id, gse::registry& reg) : m_entity_id(entity_id), m_registry(reg) {}

template <typename T>
auto gse::scene::builder::with(T value) -> builder& {
	if (context* ctx = mutation_context()) {
		ctx->add_component<T>(m_entity_id, std::move(value));
		return *this;
	}

	m_registry.add_component<T>(m_entity_id, std::move(value));
	return *this;
}

auto gse::scene::builder::identify() const -> gse::id {
	return m_entity_id;
}