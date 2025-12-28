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
		explicit world(std::string_view name = "Unnamed World");

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
	private:
		friend class director;
		friend struct local_input_source;

		auto add_trigger(
			const trigger& new_trigger
		) -> void;

		gse::registry m_registry;
		std::unordered_map<gse::id, std::unique_ptr<gse::scene>> m_scenes;
		std::vector<trigger> m_triggers;
		std::optional<gse::id> m_active_scene = std::nullopt;
		bool m_networked = false;
		std::optional<gse::id> m_client_id = std::nullopt;
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

			m_source.for_each_context(
				*m_owner,
				[&](const evaluation_context& ctx) {
					if (!ctx.input || !ctx.client_id) {
						return;
					}

					actions::sample_all_channels(*ctx.input);
				}
			);

			if (const auto* active = m_owner->current_scene()) {
				active->update();
			}
		}

		auto render(
		) -> void override {
			if (!m_owner->networked()) {
				return;
			}

			if (const auto* active = m_owner->current_scene()) {
				active->render();
			}
		}
	private:
		InputSource m_source;
	};

	struct local_input_source {
		id owner_id{};

		template <typename Fn>
		auto for_each_context(
			world& w,
			Fn&& fn
		) const -> void {
			evaluation_context ctx{
				.client_id = owner_id,
				.input = &actions::current_state(),
				.registry = &w.m_registry
			};

			fn(ctx);
		}
	};
}

gse::director::director(world* world) : m_world(world) {}

auto gse::director::when(const trigger& trigger) -> director& {
	m_world->add_trigger(trigger);
	return *this;
}

gse::world::world(const std::string_view name) : hookable(name) {
	struct default_world : hook<world> {
		using hook::hook;

		auto update() -> void override {
			if (m_owner->m_networked) {
				return;
			}

			actions::sample_all_channels(actions::current_state());

			for (const auto& [scene_id, condition] : m_owner->m_triggers) {
				const evaluation_context ctx{
					.client_id = m_owner->m_client_id,
					.input = &actions::current_state(),
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

						actions::finalize_bindings();

						break;
					}
				}
			}

			if (m_owner->m_active_scene.has_value()) {
				if (const auto* active_scene = m_owner->scene(m_owner->m_active_scene.value())) {
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

auto gse::world::direct() -> director {
	m_triggers.clear();
	m_active_scene.reset();
	m_scenes.clear();

	return director(this);
}

template <typename... Hooks, typename... Args>
auto gse::world::add(Args&&... args) -> gse::scene* {
	auto new_scene = std::make_unique<gse::scene>(m_registry, std::forward<Args>(args)...);
	(new_scene->add_hook<Hooks>(), ...);

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
		new_scene->add_hook<default_scene>();
		new_scene->initialize();
		new_scene->set_active(true);
		m_active_scene = new_scene->id();
		actions::finalize_bindings();
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

auto gse::world::add_trigger(const trigger& new_trigger) -> void {
	m_triggers.push_back(new_trigger);
}