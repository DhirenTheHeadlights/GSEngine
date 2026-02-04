export module gse.runtime:world;

import std;

import gse.assert;
import gse.utility;
import gse.platform;
import gse.physics;
import gse.graphics;

export namespace gse {
	struct evaluation_context {
		std::optional<id> client_id = std::nullopt;
		const actions::state* input = nullptr;
		registry* registry = nullptr;
	};

	struct trigger {
		id scene_id;
		std::function<bool(const evaluation_context&)> condition;
	};

	class world;

	class director {
	public:
		explicit director(
			world* world = nullptr
		);

		auto when(
			const trigger& trigger
		) -> director&;
	private:
		world* m_world = nullptr;
	};

	class world final : public hookable<world> {
	public:
		explicit world(
			system_provider& provider,
			std::string_view name = "Unnamed World"
		);

		auto direct(
		) -> director;

		template <typename... Hooks, typename... Args>
		auto add(
			Args&&... args
		) -> scene*;

		auto activate(
			const gse::id& scene_id
		) -> void;

		auto deactivate(
			const gse::id& scene_id
		) -> void;

		auto scene(
			const gse::id& scene_id
		) -> scene*;

		auto current_scene(
		) -> gse::scene*;

		auto set_networked(
			bool is_networked
		) -> void;

		auto triggers(
		) -> const std::vector<trigger>&;

		auto networked(
		) const -> bool;

		auto set_authoritative(
			bool is_authoritative
		) -> void;

		auto authoritative(
		) const -> bool;

		auto set_local_controlled_entity(
			gse::id entity_id
		) -> void;

		auto local_controlled_entity(
		) const -> gse::id;

		auto registry(
		) -> registry&;

		template <typename State>
		auto state_of(
		) -> State&;
	private:
		friend class director;
		friend struct local_input_source;

		auto add_trigger(
			const trigger& new_trigger
		) -> void;

		system_provider* m_provider = nullptr;

		gse::registry m_registry;
		std::unordered_map<gse::id, std::unique_ptr<gse::scene>> m_scenes;
		std::vector<trigger> m_triggers;
		std::optional<gse::id> m_active_scene = std::nullopt;
		bool m_networked = false;
		bool m_authoritative = true;
		std::optional<gse::id> m_client_id = std::nullopt;
		gse::id m_local_controlled_entity{};
	};

	template <typename InputSource>
	class networked_world final : public hook<world> {
	public:
		using hook::hook;

		explicit networked_world(
			world* owner,
			InputSource source
		) : hook(owner), m_source(std::move(source)) {}

		auto update(
		) -> void override {
			if (!m_owner->networked()) {
				return;
			}

			auto& a = m_owner->state_of<actions::system_state>();

			m_source.for_each_context(
				*m_owner,
				[&](const evaluation_context& ctx) {
					if (!ctx.input || !ctx.client_id) {
						return;
					}

					a.sample_for_entity(*ctx.input, *ctx.client_id);
				}
			);

			if (auto* active = m_owner->current_scene()) {
				active->update();
			}
		}

		auto render(
		) -> void override {
			if (!m_owner->networked()) {
				return;
			}

			if (auto* active = m_owner->current_scene()) {
				active->render();
			}
		}
	private:
		InputSource m_source;
	};

	struct local_input_source {
		template <typename Fn>
		auto for_each_context(
			world& w,
			Fn&& fn
		) const -> void {
			const auto local_id = w.local_controlled_entity();
			if (!local_id.exists()) {
				return;
			}

			const auto& a = w.state_of<actions::system_state>();

			evaluation_context ctx{
				.client_id = local_id,
				.input = std::addressof(a.current_state()),
				.registry = &w.m_registry
			};

			fn(ctx);
		}
	};

	class player_controller_hook : public hook<world> {
	public:
		using hook::hook;

		auto update(
		) -> void override {
			auto* current = m_owner->current_scene();
			if (!current) {
				return;
			}

			const auto& factory = current->get_player_factory();
			if (!factory) {
				return;
			}

			auto& reg = current->registry();
			const bool is_server = m_owner->authoritative();

			for (auto& pc : reg.linked_objects_write<player_controller>()) {
				const auto controller_id = pc.owner_id();

				if (is_server) {
					if (pc.controlled_entity_id.exists()) {
						continue;
					}

					const auto player_id = factory(*current);
					pc.controlled_entity_id = player_id;
					reg.mark_component_updated<player_controller>(controller_id);
				}
				else {
					if (!pc.controlled_entity_id.exists()) {
						continue;
					}

					if (m_processed.contains(controller_id)) {
						continue;
					}

					const auto local_player_id = factory(*current);
					m_owner->set_local_controlled_entity(local_player_id);
					m_processed.insert(controller_id);
				}
			}
		}
	private:
		std::unordered_set<id> m_processed;
	};
}

gse::director::director(world* world) : m_world(world) {}

auto gse::director::when(const trigger& trigger) -> director& {
	m_world->add_trigger(trigger);
	return *this;
}

gse::world::world(system_provider& provider, const std::string_view name)
	: hookable(name),
	  m_provider(std::addressof(provider)) {
	struct default_world : hook<world> {
		using hook::hook;

		auto update() -> void override {
			if (m_owner->m_networked) {
				return;
			}

			const auto& a = m_owner->state_of<actions::system_state>();
			const auto& s = a.current_state();

			a.sample_all_channels(s);

			for (const auto& [scene_id, condition] : m_owner->m_triggers) {
				const evaluation_context ctx{
					.client_id = m_owner->m_client_id,
					.input = std::addressof(s),
					.registry = &m_owner->m_registry
				};

				if (condition(ctx) && scene_id != m_owner->m_active_scene) {
					if (m_owner->m_active_scene.has_value()) {
						if (auto* old_scene = m_owner->scene(m_owner->m_active_scene.value())) {
							old_scene->set_active(false);
							old_scene->shutdown();
						}
					}

					if (auto* new_scene = m_owner->scene(scene_id)) {
						new_scene->add_hook<default_scene>();
						new_scene->initialize();
						new_scene->set_active(true);
						m_owner->m_active_scene = new_scene->id();
						break;
					}
				}
			}

			if (m_owner->m_active_scene.has_value()) {
				if (auto* active_scene = m_owner->scene(m_owner->m_active_scene.value())) {
					active_scene->update();
				}
			}
		}

		auto render() -> void override {
			for (const auto& scene : m_owner->m_scenes | std::views::values) {
				if (scene->active()) {
					scene->render();
				}
			}
		}

		auto shutdown() -> void override {
			for (const auto& scene : m_owner->m_scenes | std::views::values) {
				if (scene->active()) {
					scene->shutdown();
				}
			}
			m_owner->m_scenes.clear();
			m_owner->m_active_scene.reset();
		}
	};

	add_hook<default_world>();
}

template <typename State>
auto gse::world::state_of() -> State& {
	assert(
		m_provider != nullptr,
		std::source_location::current(),
		"world has no system_provider bound."
	);

	auto* p = m_provider->system_ptr(std::type_index(typeid(State)));
	assert(
		p != nullptr,
		std::source_location::current(),
		"requested state is not registered."
	);

	return *static_cast<State*>(p);
}

auto gse::world::direct() -> director {
	m_triggers.clear();
	m_active_scene.reset();
	m_scenes.clear();

	return director(this);
}

template <typename... Hooks, typename... Args>
auto gse::world::add(Args&&... args) -> gse::scene* {
	auto new_scene = std::make_unique<gse::scene>(m_registry, std::forward<Args>(args)...);
	(new_scene->template add_hook<Hooks>(), ...);

	auto* scene_ptr = new_scene.get();
	m_scenes[scene_ptr->id()] = std::move(new_scene);
	return scene_ptr;
}

auto gse::world::activate(const gse::id& scene_id) -> void {
	assert(
		m_networked,
		std::source_location::current(),
		"Cannot force activate scene in a non-networked world."
	);

	if (m_active_scene.has_value() && m_active_scene.value() == scene_id) {
		if (auto* old_scene = scene(m_active_scene.value())) {
			old_scene->set_active(false);
			old_scene->shutdown();
		}
	}

	if (auto* new_scene = scene(scene_id)) {
		auto& a = state_of<actions::system_state>();

		new_scene->add_hook<default_scene>();
		new_scene->initialize();
		new_scene->set_active(true);
		m_active_scene = new_scene->id();
	}
}

auto gse::world::deactivate(const gse::id& scene_id) -> void {
	assert(
		scene_id == m_active_scene,
		std::source_location::current(),
		"Can only force deactivate the currently active scene."
	);

	assert(
		m_networked,
		std::source_location::current(),
		"Cannot force deactivate a scene in a non-networked world"
	);

	if (m_active_scene.has_value()) {
		scene(m_active_scene.value())->shutdown();
	}

	m_active_scene = std::nullopt;
}

auto gse::world::scene(const gse::id& scene_id) -> gse::scene* {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		return scene->second.get();
	}
	return nullptr;
}

auto gse::world::current_scene() -> gse::scene* {
	if (const auto scene = m_active_scene; scene.has_value()) {
		return this->scene(scene.value());
	}
	return nullptr;
}

auto gse::world::set_networked(const bool is_networked) -> void {
	m_networked = is_networked;
}

auto gse::world::triggers() -> const std::vector<trigger>& {
	return m_triggers;
}

auto gse::world::networked() const -> bool {
	return m_networked;
}

auto gse::world::set_authoritative(const bool is_authoritative) -> void {
	m_authoritative = is_authoritative;
}

auto gse::world::authoritative() const -> bool {
	return m_authoritative;
}

auto gse::world::set_local_controlled_entity(const gse::id entity_id) -> void {
	m_local_controlled_entity = entity_id;
}

auto gse::world::local_controlled_entity() const -> gse::id {
	return m_local_controlled_entity;
}

auto gse::world::registry() -> gse::registry& {
	return m_registry;
}

auto gse::world::add_trigger(const trigger& new_trigger) -> void {
	m_triggers.push_back(new_trigger);
}
