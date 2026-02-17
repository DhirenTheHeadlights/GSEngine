export module gse.graphics:material_compiler;

import std;

import gse.platform;
import gse.assert;
import gse.math;

import :material;

export template<>
struct gse::asset_compiler<gse::material> {
    static auto source_extensions() -> std::vector<std::string> {
        return { ".mtl" };
    }

    static auto baked_extension() -> std::string {
        return ".gmat";
    }

    static auto source_directory() -> std::string {
        return "";
    }

    static auto baked_directory() -> std::string {
        return "Materials";
    }

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        std::ifstream mtl_file(source);
        if (!mtl_file.is_open()) {
            std::println("Failed to open material file: {}", source.string());
            return false;
        }

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
            const auto source_texture_path = weakly_canonical(source.parent_path() / texture_path_in_mtl);

            if (!std::filesystem::exists(source_texture_path)) {
                std::println("Warning: Texture '{}' referenced in '{}' was not found. Skipping.",
                    texture_path_in_mtl, source.string());
                return "";
            }

            return source_texture_path.filename().replace_extension(".gtx").string();
        };

        const auto baked_material_root = destination.parent_path();
        std::filesystem::create_directories(baked_material_root);

        std::string current_material_name;
        material_parse_data current_material_data;
        std::string diffuse_tex_path, normal_tex_path, specular_tex_path;
        bool any_written = false;

        auto write_material_file = [&](const std::string& name, const material_parse_data& mat_data,
            const std::string& diffuse_path, const std::string& normal_path, const std::string& specular_path) {
            const auto baked_path = baked_material_root / (name + ".gmat");

            std::ofstream out_file(baked_path, std::ios::binary);
            if (!out_file.is_open()) {
                return false;
            }

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

            std::println("Material compiled: {}", baked_path.filename().string());
            return true;
        };

        auto process_and_write_previous = [&] {
            if (!current_material_name.empty()) {
                if (write_material_file(current_material_name, current_material_data,
                    diffuse_tex_path, normal_tex_path, specular_tex_path)) {
                    any_written = true;
                }
                diffuse_tex_path.clear();
                normal_tex_path.clear();
                specular_tex_path.clear();
            }
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
        return any_written;
    }

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        if (!std::filesystem::exists(destination)) {
            return true;
        }
        return std::filesystem::last_write_time(source) >
               std::filesystem::last_write_time(destination);
    }

    static auto dependencies(
        const std::filesystem::path&
    ) -> std::vector<std::filesystem::path> {
        return {};
    }
};
