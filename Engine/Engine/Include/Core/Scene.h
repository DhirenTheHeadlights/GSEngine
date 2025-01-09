#pragma once

#include <string>
#include <vector>

#include "Core/Object/Object.h"
#include "Graphics/3D/Renderer3D.h"
#include "Object/Hook.h"

namespace gse {
	class scene final : public hookable<scene>, public identifiable {
	public:
		scene(const std::string& name = "Unnamed Scene") : identifiable(name) {}

		auto add_object(gse::object* object, const std::string& name) -> void;
		auto remove_object(const std::string& name) -> void;

		auto initialize() -> void;
		auto update() const -> void;
		auto render() const -> void;
		auto exit() const -> void;

		auto set_active(const bool is_active) -> void { this->m_is_active = is_active; }
		auto get_active() const -> bool { return m_is_active; }

		auto get_objects() const -> std::vector<object*>;

	private:
		std::vector<std::uint32_t> m_object_indexes;
		std::vector<object*> m_objects_to_add_upon_initialization;

		bool m_is_active = false;
	};
}