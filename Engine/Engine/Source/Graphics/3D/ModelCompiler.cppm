export module gse.graphics:model_compiler;

import std;

import gse.platform;
import gse.assert;

import :asset_compiler;
import :model;
import :mesh;

export template<>
struct gse::asset_compiler<gse::model> {
    static auto source_extensions() -> std::vector<std::string> {
        return { ".obj" };
    }

    static auto baked_extension() -> std::string {
        return ".gmdl";
    }

    static auto source_directory() -> std::string {
        return "Models";
    }

    static auto baked_directory() -> std::string {
        return "Models";
    }

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool {
        std::ifstream model_file(source);
        if (!model_file.is_open()) {
            std::println("Failed to open model file: {}", source.string());
            return false;
        }

        auto split = [](const std::string& str, const char delimiter = ' ') -> std::vector<std::string> {
            std::vector<std::string> tokens;
            const size_t length = str.length();
            size_t i = 0;
            while (i < length) {
                while (i < length && str[i] == delimiter) { ++i; }
                const size_t start = i;
                while (i < length && str[i] != delimiter) { ++i; }
                if (start < i) { tokens.emplace_back(str.substr(start, i - start)); }
            }
            return tokens;
        };

        struct mesh_build_data {
            std::string material_name;
            std::vector<vertex> vertices;
        };

        std::vector<mesh_build_data> built_meshes;
        std::string current_material_name;

        std::vector<vec3<length>> temp_positions;
        std::vector<unitless::vec3> temp_normals;
        std::vector<unitless::vec2> temp_tex_coords;

        std::string line;
        while (std::getline(model_file, line)) {
            const auto tokens = split(line, ' ');
            if (tokens.empty()) continue;

            if (const auto& type = tokens[0]; type == "mtllib") {
            }
            else if (type == "usemtl") {
                current_material_name = tokens[1];
                if (std::ranges::find_if(built_meshes, [&](const mesh_build_data& m) { return m.material_name == current_material_name; }) == built_meshes.end()) {
                    built_meshes.emplace_back(mesh_build_data{ .material_name = current_material_name });
                }
            }
            else if (type == "v") {
                temp_positions.push_back({ std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) });
            }
            else if (type == "vt") {
                temp_tex_coords.push_back({ std::stof(tokens[1]), std::stof(tokens[2]) });
            }
            else if (type == "vn") {
                temp_normals.push_back({ std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) });
            }
            else if (type == "f") {
                auto mesh_it = std::ranges::find_if(
                    built_meshes,
                    [&](const auto& m) {
                        return m.material_name == current_material_name;
                    }
                );

                if (mesh_it == built_meshes.end()) {
                    if (current_material_name.empty()) {
                        current_material_name = "default";
                    }
                    built_meshes.emplace_back(mesh_build_data{ .material_name = current_material_name });
                    mesh_it = std::prev(built_meshes.end());
                }

                auto& [material_name, vertices] = *mesh_it;

                auto process_face_vertex = [&](const std::string& token) {
                    const auto indices = split(token, '/');
                    vertex v;

                    if (!indices.empty() && !indices[0].empty()) {
                        if (const size_t idx = std::stoul(indices[0]) - 1; idx < temp_positions.size()) {
                            v.position = temp_positions[idx];
                        }
                    }

                    if (indices.size() > 1 && !indices[1].empty()) {
                        if (const size_t idx = std::stoul(indices[1]) - 1; idx < temp_tex_coords.size()) {
                            v.tex_coords = temp_tex_coords[idx];
                        }
                    }

                    if (indices.size() > 2 && !indices[2].empty()) {
                        if (const size_t idx = std::stoul(indices[2]) - 1; idx < temp_normals.size()) {
                            v.normal = temp_normals[idx];
                        }
                    }
                    return v;
                };

                if (tokens.size() == 4) {
                    vertices.push_back(process_face_vertex(tokens[1]));
                    vertices.push_back(process_face_vertex(tokens[2]));
                    vertices.push_back(process_face_vertex(tokens[3]));
                }
                else if (tokens.size() == 5) {
                    vertices.push_back(process_face_vertex(tokens[1]));
                    vertices.push_back(process_face_vertex(tokens[2]));
                    vertices.push_back(process_face_vertex(tokens[3]));
                    vertices.push_back(process_face_vertex(tokens[1]));
                    vertices.push_back(process_face_vertex(tokens[3]));
                    vertices.push_back(process_face_vertex(tokens[4]));
                }
            }
        }

        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out_file(destination, std::ios::binary);
        if (!out_file.is_open()) {
            return false;
        }

        constexpr uint32_t magic = 0x474D444C;
        constexpr uint32_t version = 1;
        out_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out_file.write(reinterpret_cast<const char*>(&version), sizeof(version));

        std::uint64_t mesh_count = built_meshes.size();
        out_file.write(reinterpret_cast<const char*>(&mesh_count), sizeof(mesh_count));

        for (const auto& [material_name, vertices] : built_meshes) {
            std::uint64_t mat_name_len = material_name.length();
            out_file.write(reinterpret_cast<const char*>(&mat_name_len), sizeof(mat_name_len));
            out_file.write(material_name.c_str(), mat_name_len);

            std::uint64_t vertex_count = vertices.size();
            out_file.write(reinterpret_cast<const char*>(&vertex_count), sizeof(vertex_count));
            out_file.write(reinterpret_cast<const char*>(vertices.data()), vertex_count * sizeof(vertex));
        }

        std::println("Model compiled: {}", destination.filename().string());
        return true;
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
