export module gse.utility:default_scene_hook;

import std;

import :hook;
import :scene;

export namespace gse {
	class default_scene final : public hook<scene> {
	public:
		using hook::hook;

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto render(
		) -> void override;

		auto shutdown(
		) -> void override;
	};
}

auto gse::default_scene::initialize() -> void {
	for (const auto& object_id : m_owner->m_queue) {
		m_owner->m_registry.activate(object_id);
		m_owner->m_entities.push_back(object_id);
	}

	m_owner->m_queue.clear();

	for (auto& [id, fn] : m_owner->m_pending_inits) {
		fn(id, m_owner->m_registry);
	}
	m_owner->m_pending_inits.clear();
}

auto gse::default_scene::update() -> void {}

auto gse::default_scene::render() -> void {}

auto gse::default_scene::shutdown() -> void {
	for (const auto& id : m_owner->m_entities) {
		m_owner->m_registry.remove(id);
	}
}
