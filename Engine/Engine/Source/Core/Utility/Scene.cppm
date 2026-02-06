export module gse.utility:scene;

import std;

import :id;
import :misc;

import :scene_hook;
import :hookable;
import :registry;

export namespace gse {
	class scene final : public hookable<scene> {
	public:
		using player_factory = std::function<gse::id(scene&)>;

		explicit scene(
			registry& registry,
			std::string_view name = "Unnamed Scene"
		);

		auto add_entity(
			const std::string& name
		) -> gse::id;

		auto remove_entity(
			const gse::id& id
		) -> void;

		auto set_active(
			bool is_active
		) -> void;

		auto active(
		) const -> bool;

		auto entities(
		) const -> std::span<const gse::id>;

		auto registry(
		) const -> registry&;

		auto set_player_factory(
			player_factory factory
		) -> void;

		auto get_player_factory(
		) const -> const player_factory&;
	private:
		gse::registry& m_registry;
		std::vector<gse::id> m_entities;
		std::vector<gse::id> m_queue;
		player_factory m_player_factory;

		friend class default_scene;

		bool m_is_active = false;
	};
}

gse::scene::scene(gse::registry& registry, const std::string_view name) 
	: hookable(name)
	, m_registry(registry) {
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
	assert(m_registry.exists(id), std::source_location::current(), "Cannot remove entity with id {}: it does not exist.", id);
	
	m_registry.remove(id);
	std::erase(m_entities, id);
}

auto gse::scene::set_active(const bool is_active) -> void {
	this->m_is_active = is_active;
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

auto gse::scene::set_player_factory(player_factory factory) -> void {
	m_player_factory = std::move(factory);
}

auto gse::scene::get_player_factory() const -> const player_factory& {
	return m_player_factory;
}
