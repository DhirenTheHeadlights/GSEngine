export module gse.graphics:material;

import std;

import :texture;
import :shader;
import :rendering_context;

import gse.physics.math;
import gse.utility;
import gse.platform;

export namespace gse {
    struct material {
        struct handle {
			handle(const material& mat) : data(&mat) {}
            const material* data = nullptr;
            auto operator->() const -> const material* { return data; }
            auto exists() const -> bool { return data != nullptr; }
        };

		material(const std::filesystem::path& path) : path(path) {}

        auto load(renderer::context& context) -> void;
        auto unload() -> void;

        unitless::vec3 ambient = unitless::vec3(1.0f);
        unitless::vec3 diffuse = unitless::vec3(1.0f);
        unitless::vec3 specular = unitless::vec3(1.0f);
        unitless::vec3 emission = unitless::vec3(0.0f);

        float shininess = 0.0f;
        float optical_density = 1.0f;
        float transparency = 1.0f;
        int illumination_model = 0;

        id diffuse_texture;
        id normal_texture;
        id specular_texture;

		std::filesystem::path path;
    };

    using material_loader = resource_loader<material, material::handle, renderer::context>;
}

auto gse::material::load(renderer::context& context) -> void {
    auto split = [](const std::string& str, const char delimiter = ' ') -> std::vector<std::string> {
        std::vector<std::string> tokens;
        const size_t length = str.length();
        size_t i = 0;
        while (i < length) {
            while (i < length && str[i] == delimiter) {
                ++i;
            }
            const size_t start = i;
            while (i < length && str[i] != delimiter) {
                ++i;
            }
            if (start < i) {
                tokens.emplace_back(str.substr(start, i - start));
            }
        }
        return tokens;
        };

    std::ifstream mtl_file(path);
    assert(mtl_file.is_open(), "Failed to open material file.");

    const std::string directory_path = path.parent_path().string() + "/";
    std::string line;

    while (std::getline(mtl_file, line)) {
        const auto tokens = split(line, ' ');
        if (tokens.empty() || tokens[0] == "#" || tokens[0] == "newmtl") {
            continue;
        }

        if (tokens[0] == "Ns") {
            shininess = std::stof(tokens[1]);
        }
        else if (tokens[0] == "Ka") {
            ambient = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
        }
        else if (tokens[0] == "Kd") {
            diffuse = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
        }
        else if (tokens[0] == "Ks") {
            specular = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
        }
        else if (tokens[0] == "Ke") {
            emission = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
        }
        else if (tokens[0] == "Ni") {
            optical_density = std::stof(tokens[1]);
        }
        else if (tokens[0] == "d") {
            transparency = std::stof(tokens[1]);
        }
        else if (tokens[0] == "illum") {
            illumination_model = std::stoi(tokens[1]);
        }
        else if (tokens[0] == "map_Kd") {
            diffuse_texture = context.queue<texture>(directory_path + tokens[1]);
        }
        else if (tokens[0] == "map_Bump") {
            normal_texture = context.queue<texture>(directory_path + tokens[1]);
        }
        else if (tokens[0] == "map_Ks") {
            specular_texture = context.queue<texture>(directory_path + tokens[1]);
        }
    }
}

auto gse::material::unload() -> void {
	diffuse_texture = {};
	normal_texture = {};
	specular_texture = {};
}

