export module gse.graphics:material;

import std;

import :texture;
import :shader;
import :rendering_context;

import gse.physics.math;
import gse.utility;
import gse.platform;

export namespace gse {
	struct material : identifiable {
		material(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path), path(path) {}
        material(
            const std::string_view name,
            const unitless::vec3 ambient,
            const unitless::vec3 diffuse,
            const unitless::vec3 specular,
            const unitless::vec3 emission,
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

        auto load(const renderer::context& context) -> void;

        auto unload() -> void;

        unitless::vec3 ambient = unitless::vec3(1.0f);
        unitless::vec3 diffuse = unitless::vec3(1.0f);
        unitless::vec3 specular = unitless::vec3(1.0f);
        unitless::vec3 emission = unitless::vec3(0.0f);

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

auto gse::material::load(const renderer::context& context) -> void {
	if (path.empty()) {
		return;
	}

	std::ifstream in_file(path, std::ios::binary);
	assert(in_file.is_open(), std::source_location::current(), "Failed to open baked material file for reading: {}", path.string());

	auto read_string = [](std::ifstream& stream) -> std::string {
		uint64_t len = 0;
		stream.read(reinterpret_cast<char*>(&len), sizeof(len));
		if (len > 0) {
			std::string str(len, '\0');
			stream.read(&str[0], len);
			return str;
		}
		return "";
	};

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

	uint32_t magic, version;
	in_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	in_file.read(reinterpret_cast<char*>(&version), sizeof(version));
	in_file.read(reinterpret_cast<char*>(&ambient), sizeof(ambient));
	in_file.read(reinterpret_cast<char*>(&diffuse), sizeof(diffuse));
	in_file.read(reinterpret_cast<char*>(&specular), sizeof(specular));
	in_file.read(reinterpret_cast<char*>(&emission), sizeof(emission));
	in_file.read(reinterpret_cast<char*>(&shininess), sizeof(shininess));
	in_file.read(reinterpret_cast<char*>(&optical_density), sizeof(optical_density));
	in_file.read(reinterpret_cast<char*>(&transparency), sizeof(transparency));
	in_file.read(reinterpret_cast<char*>(&illumination_model), sizeof(illumination_model));

	if (const auto diffuse_filename = read_string(in_file); !diffuse_filename.empty()) {
		diffuse_texture = context.get<texture>(make_texture_path(diffuse_filename));
	}
	if (const auto normal_filename = read_string(in_file); !normal_filename.empty()) {
		normal_texture = context.get<texture>(make_texture_path(normal_filename));
	}
	if (const auto specular_filename = read_string(in_file); !specular_filename.empty()) {
		specular_texture = context.get<texture>(make_texture_path(specular_filename));
	}
}

auto gse::material::unload() -> void {
	diffuse_texture = {};
	normal_texture = {};
	specular_texture = {};
}

