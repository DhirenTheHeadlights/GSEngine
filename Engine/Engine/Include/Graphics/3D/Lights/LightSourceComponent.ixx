export module gse.graphics.light_source_component;

import std;

import gse.core.component;
import gse.graphics.light;
import gse.physics.math;

export namespace gse {
	class light_source_component final : public component {
	public:
		light_source_component(const std::uint32_t id) : component(id) {}
		light_source_component(const std::uint32_t id, std::vector<std::unique_ptr<light>> lights) : component(id), m_lights(std::move(lights)) {}

		light_source_component(light_source_component&&) noexcept = default;
		auto operator=(light_source_component&&) noexcept -> light_source_component& = default;

		light_source_component(const light_source_component&) = delete;
		auto operator=(const light_source_component&) -> light_source_component& = delete;

		auto add_light(std::unique_ptr<light> light) -> void {
			m_lights.push_back(std::move(light));
		}

		auto remove_light(const std::unique_ptr<light>& light) -> void {
			std::erase(m_lights, light);
		}

		auto get_render_queue_entries() const -> std::vector<light_render_queue_entry> {
			std::vector<light_render_queue_entry> entries;
			entries.reserve(m_lights.size());
			for (const auto& light : m_lights) {
				entries.push_back(light->get_render_queue_entry());
			}
			return entries;
		}

		auto get_lights() const -> std::vector<light*> {
			return get_pointers(m_lights);
		}
	private:
		std::vector<std::unique_ptr<light>> m_lights;
	};
}
