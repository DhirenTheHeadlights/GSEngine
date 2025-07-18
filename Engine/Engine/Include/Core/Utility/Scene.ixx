export module gse.utility:scene;

import std;

import :registry;
import :misc;

import gse.assert;

export namespace gse {
	class scene final : public hookable<scene> {
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

template <typename T>
auto gse::hook<gse::entity>::component() -> T& {
	return m_scene->registry().linked_object<T>(owner_id());
}

template <typename T>
auto gse::hook<gse::entity>::try_component() -> T* {
	return m_scene->registry().try_linked_object<T>(owner_id());
}

gse::scene::scene(const std::string& name) : hookable(name) {
	class default_scene final : public hook<scene> {
	public:
		explicit default_scene(scene* owner) : hook(owner) {}

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

	add_hook(std::make_unique<default_scene>(this));
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

