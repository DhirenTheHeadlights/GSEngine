export module gse.utility:world;

import std;

import :ecs;

export namespace gse {
	struct trigger {
		id scene_id;
		std::function<bool()> condition;
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

		auto direct(
		) -> director;

		auto add(
			std::string_view name = "Unnamed Scene"
		) -> scene*;

		auto scene(
			const gse::id& scene_id
		) -> scene*;
	private:
		friend class director;
		auto add_trigger(const trigger& new_trigger) -> void;

		std::unordered_map<gse::id, std::unique_ptr<gse::scene>> m_scenes;
		std::vector<trigger> m_triggers;
		std::optional<gse::id> m_active_scene;
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
		        if (condition() && scene_id != m_owner->m_active_scene) {
					if (m_owner->m_active_scene.has_value()) {
		                if (auto* old_scene = m_owner->scene(m_owner->m_active_scene.value())) {
		                    old_scene->set_active(false);
		                    old_scene->shutdown();
		                }
		            }

		            if (auto* new_scene = m_owner->scene(scene_id)) {
		                new_scene->add_hook(std::make_unique<default_scene>(new_scene));
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

auto gse::world::direct() -> director {
	return director(this);
}

auto gse::world::add(const std::string_view name) -> gse::scene* {
	auto new_scene = std::make_unique<gse::scene>(name);
    auto* scene_ptr = new_scene.get();
    m_scenes[scene_ptr->id()] = std::move(new_scene);
    return scene_ptr;
}

auto gse::world::scene(const gse::id& scene_id) -> gse::scene* {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		return scene->second.get();
	}
	return nullptr;
}

auto gse::world::add_trigger(const trigger& new_trigger) -> void {
	m_triggers.push_back(new_trigger);
}
