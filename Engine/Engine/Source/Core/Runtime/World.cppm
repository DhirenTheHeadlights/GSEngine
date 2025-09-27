export module gse.runtime:world;

import std;

import gse.assert;
import gse.utility;
import gse.platform;

export namespace gse {
	struct evaluation_context {
		id client_id;
		const input_state& input;
		const registry& registry;
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
		world(std::string_view name = "Unnamed World");

		auto flip_registry_buffers(
		) -> void;

		auto direct(
		) -> director;

		template <typename... Hooks>
		auto add(
			std::string_view name = "Unnamed Scene"
		) -> scene*;

		auto activate(
			const gse::id& scene_id
		) -> void;

		auto scene(
			const gse::id& scene_id
		) -> scene*;

		auto current_scene(
		) -> gse::scene*;

		auto registries(
		) -> std::vector<std::reference_wrapper<registry>>;

		auto set_networked(
			bool is_networked
		) -> void;

		auto triggers() -> const std::vector<trigger>&;
	private:
		friend class director;
		auto add_trigger(const trigger& new_trigger) -> void;

		std::unordered_map<gse::id, std::unique_ptr<gse::scene>> m_scenes;
		std::vector<trigger> m_triggers;
		std::optional<gse::id> m_active_scene;
		bool m_networked = false;
	};
}

gse::director::director(world* world) : m_world(world) {}

auto gse::director::when(const trigger& trigger) -> director& {
	m_world->add_trigger(trigger);
	return *this;
}

gse::world::world(const std::string_view name): hookable(name) {
	struct default_world_hook : hook<world> {
		using hook::hook;

		auto update() -> void override {
			for (const auto& [scene_id, condition] : m_owner->m_triggers) {
		        if (!m_owner->m_networked && condition() && scene_id != m_owner->m_active_scene) {
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
}

auto gse::world::flip_registry_buffers() -> void {
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->active()) {
			scene->registry().flip_buffers();
		}
	}
}

auto gse::world::direct() -> director {
	return director(this);
}

template <typename... Hooks>
auto gse::world::add(const std::string_view name) -> gse::scene* {
	auto new_scene = std::make_unique<gse::scene>(name);
	(new_scene->add_hook<Hooks>(), ...);

    auto* scene_ptr = new_scene.get();
    m_scenes[scene_ptr->id()] = std::move(new_scene);
    return scene_ptr;
}

auto gse::world::activate(const gse::id& scene_id) -> void {
	assert(
		m_networked,
		"Cannot force a scene to activate scene in a non-networked world."
	);

	if (m_active_scene.has_value()) {
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
	}
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

auto gse::world::registries() -> std::vector<std::reference_wrapper<registry>> {
	std::vector<std::reference_wrapper<registry>> active_registries;
	active_registries.reserve(m_scenes.size());

	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->active()) {
			active_registries.push_back(std::ref(scene->registry()));
		}
	}

	return active_registries;
}

auto gse::world::set_networked(const bool is_networked) -> void {
	m_networked = is_networked;
}

auto gse::world::triggers() -> const std::vector<trigger>& {
	return m_triggers;
}

auto gse::world::add_trigger(const trigger& new_trigger) -> void {
	m_triggers.push_back(new_trigger);
}
