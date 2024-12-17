#pragma once

#include "Core/Scene.h"

namespace gse {
	class scene_handler {
	public:
		scene_handler() = default;

		void add_scene(std::unique_ptr<gse::scene>& scene, const std::string& tag);
		void remove_scene(const std::shared_ptr<id>& scene_id);

		void activate_scene(const std::shared_ptr<id>& scene_id);
		void deactivate_scene(const std::shared_ptr<id>& scene_id);
		void update();
		void render();
		void exit();

		void set_engine_initialized(const bool initialized) { this->m_engine_initialized = initialized; }
		void queue_scene_trigger(const std::shared_ptr<id>& id, const std::function<bool()>& trigger);

		std::vector<scene*> get_active_scenes() const;
		std::vector<std::shared_ptr<id>> get_all_scenes() const;
		std::vector<std::shared_ptr<id>> get_active_scene_ids() const;
		scene* get_scene(const std::shared_ptr<id>& scene_id) const;
	private:
		std::optional<GLuint> m_fbo = std::nullopt;
		bool m_engine_initialized = false;

		std::unordered_map<std::shared_ptr<id>, std::unique_ptr<scene>> m_scenes;
		std::unordered_map<std::shared_ptr<id>, std::function<bool()>> m_scene_triggers;
	};
}