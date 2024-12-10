#pragma once

#include <string>
#include <vector>

#include "Core/Object/Object.h"
#include "Graphics/3D/Renderer3D.h"
#include "Physics/System.h"
#include "Physics/Collision/BroadPhaseCollisions.h"

namespace gse {
	class scene {
	public:
		scene() = default;
		scene(const std::shared_ptr<id>& id) : m_id(id) {}

		void add_object(const std::weak_ptr<object>& object);
		void remove_object(const std::weak_ptr<object>& object);

		void initialize();
		void update();
		void render();
		void exit();

		void set_id(const std::shared_ptr<id>& id) { this->m_id = id; }
		std::shared_ptr<id> get_id() const { return m_id; }

		void set_active(const bool is_active) { this->m_is_active = is_active; }
		bool get_active() const { return m_is_active; }
	private:
		std::vector<std::weak_ptr<object>> m_objects;

		bool m_is_active = false;

		std::shared_ptr<id> m_id;

		physics::group m_physics_system;
		broad_phase_collision::group m_collision_group;
		renderer::group m_render_group;
	};
}