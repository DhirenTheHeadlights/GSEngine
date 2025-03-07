export module gse.graphics.material;

import std;

import gse.core.id;
import gse.graphics.texture;
import gse.graphics.shader;
import gse.physics.math;
import gse.platform.assert;

namespace gse {
	struct material {
		unitless::vec3 ambient = unitless::vec3(1.0f);
		unitless::vec3 diffuse = unitless::vec3(1.0f);
		unitless::vec3 specular = unitless::vec3(1.0f);
		unitless::vec3 emission = unitless::vec3(0.0f);

		float shininess = 0.0f;
		float optical_density = 1.0f;
		float transparency = 1.0f;
		int illumination_model = 0;

		uuid diffuse_texture;
		uuid normal_texture;
		uuid specular_texture;
	};
}

export namespace gse {
	class material_handle {
	public:
		material_handle(material* material = nullptr) : m_material(material) {}

		auto operator->() const -> const material* {
			return m_material;
		}

		auto get_data() const -> material* {
			return m_material;
		}
	private:
		material* m_material = nullptr;
	};

	auto get_material(const std::string& name) -> material_handle;
	auto does_material_exist(const std::string& name) -> bool;
	auto generate_material(const unitless::vec3& ambient, const unitless::vec3& diffuse, const unitless::vec3& specular, const unitless::vec3& emission, float shininess, float optical_density, float transparency, int illumination_model, const std::string& name = "") -> material_handle;
	auto generate_material(uuid diffuse_texture, uuid normal_texture, uuid specular_texture, const std::string& name = "") -> material_handle;
}

std::unordered_map<std::string, gse::material> g_materials;
std::unordered_map<std::string, std::vector<gse::material_handle>> g_materials_handles;

auto gse::get_material(const std::string& name) -> material_handle {
	perma_assert(g_materials.contains(name), "Material not found.");
	return &g_materials[name];
}

auto gse::does_material_exist(const std::string& name) -> bool {
	return g_materials.contains(name);
}

auto gse::generate_material(const unitless::vec3& ambient, const unitless::vec3& diffuse, const unitless::vec3& specular, const unitless::vec3& emission, const float shininess, const float optical_density, const float transparency, const int illumination_model, const std::string& name) -> material_handle {
	auto [it, inserted] = g_materials.emplace(name, material{ ambient, diffuse, specular, emission, shininess, optical_density, transparency, illumination_model, -1, -1, -1 });
	return  &it->second;
}

auto gse::generate_material(const uuid diffuse_texture, const uuid normal_texture, const uuid specular_texture, const std::string& name) -> material_handle {
	if (g_materials.contains(name)) {
		g_materials[name].diffuse_texture = diffuse_texture;
	}

	auto [it, inserted] = g_materials.emplace(name, material{ unitless::vec3(1.0f), unitless::vec3(1.0f), unitless::vec3(1.0f), unitless::vec3(0.0f), 0.0f, 1.0f, 1.0f, 0, diffuse_texture, normal_texture, specular_texture });
	return  &it->second;
}
