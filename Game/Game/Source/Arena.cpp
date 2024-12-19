#include "Arena.h"

struct wall_hook final : gse::hook<> {
	using hook::hook;

	void initialize() override {
		m_owner->get_component<gse::physics::motion_component>()->affected_by_gravity = false;
	}
};

void game::arena::create(gse::scene* scene) {
	const auto arena_position = gse::vec3<gse::units::meters>(0.f, 0.f, 0.f);

	const gse::length arena_size = gse::meters(1000.f);
	const gse::length wall_thickness = gse::meters(1.f);

	scene->add_object(std::make_unique<gse::box>(arena_position + gse::vec3<gse::units::meters>(0.f, 0.f, arena_size.as<gse::units::meters>() / 2.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>(), wall_thickness)));
	scene->add_object(std::make_unique<gse::box>(arena_position + gse::vec3<gse::units::meters>(0.f, arena_size.as<gse::units::meters>() / 2.f, 0.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), wall_thickness, arena_size.as<gse::units::meters>())));
	scene->add_object(std::make_unique<gse::box>(arena_position + gse::vec3<gse::units::meters>(0.f, -arena_size.as<gse::units::meters>() / 2.f, 0.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), wall_thickness, arena_size.as<gse::units::meters>())));
	scene->add_object(std::make_unique<gse::box>(arena_position + gse::vec3<gse::units::meters>(-arena_size.as<gse::units::meters>() / 2.f, 0.f, 0.f), gse::vec3<gse::units::meters>(wall_thickness, arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>())));
	scene->add_object(std::make_unique<gse::box>(arena_position + gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>() / 2.f, 0.f, 0.f), gse::vec3<gse::units::meters>(wall_thickness, arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>())));
	scene->add_object(std::make_unique<gse::box>(arena_position + gse::vec3<gse::units::meters>(0.f, 0.f, -arena_size.as<gse::units::meters>() / 2.f), gse::vec3<gse::units::meters>(arena_size.as<gse::units::meters>(), arena_size.as<gse::units::meters>(), wall_thickness)));

	for (const auto& object : scene->get_objects()) {
		object->add_hook(std::make_unique<wall_hook>(object));
	}
}
