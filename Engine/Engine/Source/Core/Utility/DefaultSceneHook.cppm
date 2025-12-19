export module gse.utility:default_scene_hook;

import std;

import :hook;
import :scene;

export namespace gse {
	class default_scene final : public hook<scene> {
	public:
		using hook::hook;

		auto initialize() -> void override;
		auto update() -> void override;
		auto render() -> void override;
		auto shutdown() -> void override;
	};
}

auto gse::default_scene::initialize() -> void {
	for (const auto& object_id : m_owner->m_queue) {
		m_owner->m_registry.activate(object_id);
		m_owner->m_entities.push_back(object_id);
	}

	m_owner->m_queue.clear();
}

auto gse::default_scene::update() -> void {
	m_owner->registry().update();
	bulk_invoke(m_owner->registry().all_hooks(), &hook<entity>::update);
}

auto gse::default_scene::render() -> void {
	m_owner->registry().render();
	bulk_invoke(m_owner->registry().all_hooks(), &hook<entity>::render);
}

auto gse::default_scene::shutdown() -> void {
	bulk_invoke(m_owner->registry().all_hooks(), &hook<entity>::shutdown);

	for (const auto& id : m_owner->m_entities) {
		m_owner->m_registry.remove(id);
	}
}