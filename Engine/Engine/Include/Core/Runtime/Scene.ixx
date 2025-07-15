export module gse.runtime:scene;

import std;

import :main_clock;

import gse.assert;
import gse.utility;
import gse.physics;

export namespace gse {
	class scene final : public hookable<scene>, public identifiable {
	public:
		explicit scene(const std::string& name = "Unnamed Scene");

		auto add_entity(const std::string& name) -> gse::id;
		auto remove_entity(const gse::id& id) -> void;

		auto set_active(const bool is_active) -> void { this->m_is_active = is_active; }
		auto active() const -> bool { return m_is_active; }
		auto entities() const -> std::span<const gse::id>;
		auto registry() -> registry& { return m_registry; }
	private:
		gse::registry m_registry;
		std::vector<gse::id> m_entities;
		std::vector<gse::id> m_queue;

		bool m_is_active = false;
	};
}

gse::scene::scene(const std::string& name) : identifiable(name) {
	struct default_scene_hook : hook<scene> {
		explicit default_scene_hook(scene* owner) : hook(owner) {}
		~default_scene_hook() override;

		auto initialize() -> void override {
			for (const auto& object_id : m_owner->m_queue) {
				m_owner->m_registry.activate(object_id);
				m_owner->m_entities.push_back(object_id);
			}

			m_owner->m_queue.clear();

			bulk_invoke(m_owner->m_registry.linked_objects<hook<entity>>(), &hook<entity>::initialize);
		}

		auto update() -> void override {
			bulk_invoke(m_owner->m_registry.linked_objects<hook<entity>>(), &hook<entity>::update);
		}

		auto render() -> void override {
			bulk_invoke(m_owner->m_registry.linked_objects<hook<entity>>(), &hook<entity>::render);
		}
	};

	add_hook(std::make_unique<default_scene_hook>(this));
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

auto gse::scene::remove_entity(const gse::id& id) -> void {
	assert(m_registry.exists(id), std::format("Entity '{}' not found in scene '{}'", id, this->id().tag()));
	m_registry.remove(id);
	std::erase(m_entities, id);
}

auto gse::scene::entities() const -> std::span<const gse::id> {
	return m_entities;
}

