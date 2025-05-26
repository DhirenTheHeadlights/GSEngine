export module gse.core.scene;

import std;

import gse.core.id;
import gse.core.object.hook;
import gse.core.object_registry;
import gse.graphics.renderer;
import gse.graphics.renderer3d;
import gse.physics.update;
import gse.physics.broad_phase_collision;

export namespace gse {
	class scene final : public hookable<scene>, public identifiable {
	public:
		scene(const std::string& name = "Unnamed Scene") : identifiable(name) {}

		auto add_entity(std::uint32_t object_uuid, const std::string& name) -> void;
		auto remove_entity(const std::string& name) -> void;

		auto initialize() -> void;
		auto update() const -> void;
		auto render() const -> void;
		auto exit() const -> void;

		auto set_active(const bool is_active) -> void { this->m_is_active = is_active; }
		auto get_active() const -> bool { return m_is_active; }

		auto get_entities() const -> std::vector<std::uint32_t>;
	private:
		std::vector<std::uint32_t> m_object_indexes;
		std::vector<std::pair<std::uint32_t, std::string>> m_objects_to_add_upon_initialization;

		bool m_is_active = false;
	};
}

auto gse::scene::add_entity(std::uint32_t object_uuid, const std::string& name) -> void {
	if (m_is_active) {
		m_object_indexes.push_back(registry::activate_entity(object_uuid, name));
	}
	else {
		m_objects_to_add_upon_initialization.emplace_back(object_uuid, name);
	}
}

auto gse::scene::remove_entity(const std::string& name) -> void {
	std::erase(m_object_indexes, registry::remove_entity(name));
}

auto gse::scene::initialize() -> void {
	initialize_hooks();

	for (const auto& [object_uuid, name] : m_objects_to_add_upon_initialization) {
		m_object_indexes.push_back(registry::activate_entity(object_uuid, name));
	}

	m_objects_to_add_upon_initialization.clear();

	registry::initialize_hooks();
	renderer3d::initialize_objects();
}

auto gse::scene::update() const -> void {
	update_hooks();

	registry::update_hooks();
	physics::update();
	renderer::update();
}

auto gse::scene::render() const -> void {
	render_hooks();

	registry::render_hooks();
}

auto gse::scene::exit() const -> void {
	for (const auto& index : m_object_indexes) {
		registry::remove_entity(index);
	}
}

auto gse::scene::get_entities() const -> std::vector<std::uint32_t> {
	return m_object_indexes;
}

