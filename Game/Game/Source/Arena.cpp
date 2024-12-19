#include "Arena.h"

#include <imgui.h>

	void initialize() override {
		m_owner->get_component<gse::physics::motion_component>()->affected_by_gravity = false;
		m_owner->get_component<gse::physics::collision_component>()->resolve_collisions = false;
	}
};

    // Common indices for each face
    const std::vector<unsigned int> face_indices = { 0, 1, 2, 2, 3, 0 };

    const auto render_component = std::make_shared<gse::render_component>(m_id.get());

    // Loop over each face, creating a Mesh and RenderComponent with a unique color
    for (size_t i = 0; i < 6; ++i) {
        auto mesh = std::make_shared<gse::mesh>(face_vertices[i], face_indices);
		mesh->set_color(colors[i]);
        render_component->add_mesh(mesh);

		auto bounding_box_mesh = std::make_shared<gse::bounding_box_mesh>(collision_component->bounding_boxes[i].lower_bound, collision_component->bounding_boxes[i].upper_bound);
		render_component->add_bounding_box_mesh(bounding_box_mesh);
    }

    add_component(render_component);
    add_component(collision_component);

    get_component<gse::render_component>()->set_render(true, true);
}

void game::arena::update() {

}

void game::arena::render() {

}
