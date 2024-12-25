#pragma once

#include <string>
#include <vector>

#include "Core/Object/Object.h"
#include "Graphics/3D/Renderer3D.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisions.h"

namespace gse {
	class scene : public hookable, public identifiable {
	public:
		scene(const std::string& name = "Unnamed Scene") : identifiable(name) {}
		scene(const std::shared_ptr<id>& id) : identifiable(id) {}

		void add_object(std::unique_ptr<object>&& object);
		void remove_object(const object* object_to_remove);

		void initialize();
		void update();
		void render();
		void exit() const;

		void set_active(const bool is_active) { this->m_is_active = is_active; }
		bool get_active() const { return m_is_active; }

		std::vector<object*> get_objects() const;

	private:
		std::vector<std::unique_ptr<object>> m_objects;

		bool m_is_active = false;

		physics::group m_physics_system;
		broad_phase_collision::group m_collision_group;
		renderer3d::group m_render_group;
	};
}