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
		material(const std::filesystem::path& path) : identifiable(path), path(path) {}
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

		static auto compile() -> std::set<std::filesystem::path>;

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

auto gse::material::compile() -> std::set<std::filesystem::path> {
	const auto source_root = config::resource_path;
	const auto baked_material_root = config::baked_resource_path / "Materials";
	const auto baked_texture_root = config::baked_resource_path / "Textures";

	if (!exists(source_root)) return {};
	if (!exists(baked_material_root)) {
		create_directories(baked_material_root);
	}

	std::println("Compiling materials...");

	struct material_parse_data {
		unitless::vec3 ambient = unitless::vec3(1.0f);
		unitless::vec3 diffuse = unitless::vec3(1.0f);
		unitless::vec3 specular = unitless::vec3(1.0f);
		unitless::vec3 emission = unitless::vec3(0.0f);
		float shininess = 0.0f;
		float optical_density = 1.0f;
		float transparency = 1.0f;
		int illumination_model = 0;
	};

	auto write_string = [](std::ofstream& stream, const std::string& str) {
		const uint64_t len = str.length();
		stream.write(reinterpret_cast<const char*>(&len), sizeof(len));
		stream.write(str.c_str(), len);
	};

	auto write_material_file = [&](const std::string& name, const material_parse_data& mat_data, const std::string& diffuse_path, const std::string& normal_path, const std::string& specular_path,  const std::filesystem::path& source_mtl_path) {
		const auto baked_path = baked_material_root / (name + ".gmat");
		if (exists(baked_path) && last_write_time(source_mtl_path) <= last_write_time(baked_path)) {
			return;
		}

		std::ofstream out_file(baked_path, std::ios::binary);
		assert(out_file.is_open(), std::source_location::current(), "Failed to open baked material file for writing: {}", baked_path.string());

		constexpr uint32_t magic = 0x474D4154;
		constexpr uint32_t version = 1;
		out_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
		out_file.write(reinterpret_cast<const char*>(&version), sizeof(version));

		out_file.write(reinterpret_cast<const char*>(&mat_data.ambient), sizeof(mat_data.ambient));
		out_file.write(reinterpret_cast<const char*>(&mat_data.diffuse), sizeof(mat_data.diffuse));
		out_file.write(reinterpret_cast<const char*>(&mat_data.specular), sizeof(mat_data.specular));
		out_file.write(reinterpret_cast<const char*>(&mat_data.emission), sizeof(mat_data.emission));
		out_file.write(reinterpret_cast<const char*>(&mat_data.shininess), sizeof(mat_data.shininess));
		out_file.write(reinterpret_cast<const char*>(&mat_data.optical_density), sizeof(mat_data.optical_density));
		out_file.write(reinterpret_cast<const char*>(&mat_data.transparency), sizeof(mat_data.transparency));
		out_file.write(reinterpret_cast<const char*>(&mat_data.illumination_model), sizeof(mat_data.illumination_model));

		write_string(out_file, diffuse_path);
		write_string(out_file, normal_path);
		write_string(out_file, specular_path);

		out_file.close();
		std::print("Material compiled: {}\n", baked_path.filename().string());
	};

	std::set<std::filesystem::path> resources;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
		if (entry.path().extension() != ".mtl") continue;

		const auto source_mtl_path = entry.path();
		std::ifstream mtl_file(source_mtl_path);
		assert(mtl_file.is_open(), std::source_location::current(), "Failed to open source material file: {}", source_mtl_path.string());

		std::string current_material_name;
		material_parse_data current_material_data;
		std::string diffuse_tex_path, normal_tex_path, specular_tex_path;

		auto process_and_write_previous = [&] {
			if (!current_material_name.empty()) {
				const auto baked_path = baked_material_root / (current_material_name + ".gmat");
				resources.insert(baked_path);

				write_material_file(
					current_material_name, current_material_data,
					diffuse_tex_path, normal_tex_path, specular_tex_path,
					source_mtl_path
				);
				diffuse_tex_path.clear();
				normal_tex_path.clear();
				specular_tex_path.clear();
			}
		};

		auto split = [](const std::string& str) -> std::vector<std::string> {
			std::vector<std::string> tokens;
			const size_t length = str.length();
			size_t i = 0;
			while (i < length) {
				while (i < length && str[i] == ' ') { ++i; }
				const size_t start = i;
				while (i < length && str[i] != ' ') { ++i; }
				if (start < i) { tokens.emplace_back(str.substr(start, i - start)); }
			}
			return tokens;
		};

		auto get_baked_texture_path = [&](const std::string& texture_path_in_mtl) -> std::string {
			const auto source_texture_path = weakly_canonical(source_mtl_path.parent_path() / texture_path_in_mtl);

			if (!std::filesystem::exists(source_texture_path)) {
				std::println(
					"Warning: Texture '{}' referenced in '{}' was not found. Skipping.",
					texture_path_in_mtl,
					source_mtl_path.string()
				);
				return "";
			}

			return source_texture_path.filename().replace_extension(".gtx").string();
		};

		std::string line;
		while (std::getline(mtl_file, line)) {
			const auto tokens = split(line);
			if (tokens.empty() || tokens[0] == "#") continue;

			if (tokens[0] == "newmtl") {
				process_and_write_previous();
				current_material_name = tokens[1];
				current_material_data = material_parse_data{};
			}
			else if (!current_material_name.empty()) {
				if (tokens[0] == "Ns") current_material_data.shininess = std::stof(tokens[1]);
				else if (tokens[0] == "Ka") current_material_data.ambient = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
				else if (tokens[0] == "Kd") current_material_data.diffuse = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
				else if (tokens[0] == "Ks") current_material_data.specular = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
				else if (tokens[0] == "Ke") current_material_data.emission = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
				else if (tokens[0] == "Ni") current_material_data.optical_density = std::stof(tokens[1]);
				else if (tokens[0] == "d") current_material_data.transparency = std::stof(tokens[1]);
				else if (tokens[0] == "illum") current_material_data.illumination_model = std::stoi(tokens[1]);
				else if (tokens[0] == "map_Kd") diffuse_tex_path = get_baked_texture_path(tokens.back());
				else if (tokens[0] == "map_Bump" || tokens[0] == "norm") normal_tex_path = get_baked_texture_path(tokens.back());
				else if (tokens[0] == "map_Ks") specular_tex_path = get_baked_texture_path(tokens.back());
			}
		}

		process_and_write_previous();
	}

	return resources;
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
		diffuse_texture = context.get<texture>(std::filesystem::path(diffuse_filename).stem().string());
	}
	if (const auto normal_filename = read_string(in_file); !normal_filename.empty()) {
		normal_texture = context.get<texture>(std::filesystem::path(normal_filename).stem().string());
	}
	if (const auto specular_filename = read_string(in_file); !specular_filename.empty()) {
		specular_texture = context.get<texture>(std::filesystem::path(specular_filename).stem().string());
	}
}

auto gse::material::unload() -> void {
	diffuse_texture = {};
	normal_texture = {};
	specular_texture = {};
}

