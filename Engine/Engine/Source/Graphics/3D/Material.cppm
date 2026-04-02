export module gse.graphics:material;

import std;

import :texture;

import gse.math;
import gse.utility;
import gse.platform;

export namespace gse {
	struct material : identifiable {
		material(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path), path(path) {}
        material(
            const std::string_view name,
            const vec3f ambient,
            const vec3f diffuse,
            const vec3f specular,
            const vec3f emission,
            const float shininess = 0.0f,
            const float optical_density = 1.0f,
            const float transparency = 1.0f,
            const int illumination_model = 0
        ) : identifiable(name),
            ambient(ambient),
			diffuse(diffuse),
			specular(specular),
			emission(emission),
			shininess(shininess),
			optical_density(optical_density),
			transparency(transparency),
			illumination_model(illumination_model) {}
        material(
			const std::string_view name,
            const resource::handle<texture>& diffuse_texture,
            const resource::handle<texture>& normal_texture = {},
            const resource::handle<texture>& specular_texture = {}
        ) : identifiable(name),
            diffuse_texture(diffuse_texture),
            normal_texture(normal_texture),
			specular_texture(specular_texture) {}

        auto load(gpu::resource_manager& context) -> void;

        auto unload() -> void;

        vec3f ambient = vec3f(1.0f);
        vec3f diffuse = vec3f(1.0f);
        vec3f specular = vec3f(1.0f);
        vec3f emission = vec3f(0.0f);

        float shininess = 0.0f;
        float optical_density = 1.0f;
        float transparency = 1.0f;
        int illumination_model = 0;

        resource::handle<texture> diffuse_texture;
        resource::handle<texture> normal_texture;
        resource::handle<texture> specular_texture;

		std::filesystem::path path;
    };
}

auto gse::material::load(gpu::resource_manager& context) -> void {
	if (path.empty()) {
		return;
	}

	std::ifstream in_file(path, std::ios::binary);
	assert(in_file.is_open(), std::source_location::current(), "Failed to open baked material file for reading: {}", path.string());
	if (!in_file.is_open()) return;

	binary_reader ar(in_file, 0x474D4154, 2, path.string());
	if (!ar.valid()) return;

	auto material_relative = path.lexically_relative(config::baked_resource_path);
	auto material_dir = material_relative.parent_path();

	std::string material_dir_str = material_dir.string();
	std::ranges::replace(material_dir_str, '\\', '/');
	if (material_dir_str.starts_with("Materials/")) {
		material_dir_str = "Textures/" + material_dir_str.substr(10);
	}

	auto make_texture_path = [&](const std::string& filename) -> std::string {
		auto stem = std::filesystem::path(filename).stem().string();
		return material_dir_str + "/" + stem;
	};

	ar & ambient & diffuse & specular & emission;
	ar & shininess & optical_density & transparency & illumination_model;

	std::string diffuse_filename, normal_filename, specular_filename;
	ar & diffuse_filename & normal_filename & specular_filename;

	if (!diffuse_filename.empty()) {
		diffuse_texture = context.get<texture>(make_texture_path(diffuse_filename));
	}
	if (!normal_filename.empty()) {
		normal_texture = context.get<texture>(make_texture_path(normal_filename));
	}
	if (!specular_filename.empty()) {
		specular_texture = context.get<texture>(make_texture_path(specular_filename));
	}
}

auto gse::material::unload() -> void {
	diffuse_texture = {};
	normal_texture = {};
	specular_texture = {};
}

