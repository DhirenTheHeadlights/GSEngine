#pragma once

#include <typeindex>
#include <vector>

#include "Core/EngineComponent.h"
#include "Core/ID.h"
#include "Core/Object/Hook.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse {
	class object : public hookable, public composable, public identifiable {
	public:
		explicit object(const std::string& name = "Unnamed Entity") : identifiable(name) {}
		virtual ~object() = default;

		void set_scene_id(const std::shared_ptr<id>& scene_id) { this->m_scene_id = scene_id; }

		std::weak_ptr<id> get_scene_id() const { return m_scene_id; }

		bool operator==(const object& other) const { return m_id == other.m_id; }
	private:
		std::shared_ptr<id> m_scene_id;
	};
}
