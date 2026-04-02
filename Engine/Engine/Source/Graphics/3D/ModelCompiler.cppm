export module gse.graphics:model_compiler;

import std;

import gse.platform;
import gse.assert;
import gse.log;
import gse.math;
import gse.utility;

import :mesh;
import :model;

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
    ) -> bool;

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

private:
    struct vertex_hash {
        auto operator()(const vertex& v) const -> std::size_t {
            auto hash_float = [](const float f) -> std::size_t {
                return std::hash<std::uint32_t>{}(std::bit_cast<std::uint32_t>(f));
            };

            std::size_t h = 0;
            auto combine = [&](const std::size_t v) {
                h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
            };

            combine(hash_float(internal::to_storage(v.position.x())));
            combine(hash_float(internal::to_storage(v.position.y())));
            combine(hash_float(internal::to_storage(v.position.z())));
            combine(hash_float(v.normal.x()));
            combine(hash_float(v.normal.y()));
            combine(hash_float(v.normal.z()));
            combine(hash_float(v.tex_coords.x()));
            combine(hash_float(v.tex_coords.y()));
            return h;
        }
    };

    struct deduped_mesh {
        std::string material_name;
        std::vector<vertex> vertices;
        std::vector<std::uint32_t> indices;
        meshlet_data meshlets;
    };

    static auto deduplicate(
        const std::string& material_name,
        const std::vector<vertex>& raw_vertices
    ) -> deduped_mesh;

    static auto build_meshlets(
        const std::vector<vertex>& vertices,
        const std::vector<std::uint32_t>& indices
    ) -> meshlet_data;

    static auto compute_meshlet_bounds(
        const std::vector<vertex>& vertices,
        const meshlet_descriptor& desc,
        std::span<const std::uint32_t> meshlet_vertex_indices,
        std::span<const std::uint8_t> meshlet_triangles
    ) -> meshlet_bounds;

};

auto gse::asset_compiler<gse::model>::compile_one(
    const std::filesystem::path& source,
    const std::filesystem::path& destination
) -> bool {
    std::ifstream model_file(source);
    if (!model_file.is_open()) {
        log::println(log::level::error, log::category::assets, "Failed to open model file: {}", source.string());
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

    std::vector<vec3<displacement>> temp_positions;
    std::vector<vec3f> temp_normals;
    std::vector<vec2f> temp_tex_coords;

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

    std::vector<deduped_mesh> meshes;
    meshes.reserve(built_meshes.size());
    for (auto& [material_name, vertices] : built_meshes) {
        meshes.push_back(deduplicate(material_name, vertices));
    }

    std::filesystem::create_directories(destination.parent_path());
    std::ofstream out_file(destination, std::ios::binary);
    if (!out_file.is_open()) {
        return false;
    }

    binary_writer ar(out_file, 0x474D444C, 3);

    const auto mesh_count = static_cast<std::uint32_t>(meshes.size());
    ar & mesh_count;

    for (auto& m : meshes) {
        ar & m.material_name;
        ar & raw_blob(m.vertices);
        ar & raw_blob(m.indices);
        ar & raw_blob(m.meshlets.descriptors);
        ar & raw_blob(m.meshlets.vertex_indices);
        ar & raw_blob(m.meshlets.triangles);
        ar & raw_blob(m.meshlets.bounds);
    }

    log::println(log::category::assets, "Model compiled: {} ({} meshes)", destination.filename().string(), mesh_count);
    return true;
}

auto gse::asset_compiler<gse::model>::deduplicate(
    const std::string& material_name,
    const std::vector<vertex>& raw_vertices
) -> deduped_mesh {
    std::unordered_map<vertex, std::uint32_t, vertex_hash> vertex_map;
    vertex_map.reserve(raw_vertices.size());

    std::vector<vertex> unique_vertices;
    std::vector<std::uint32_t> indices;
    indices.reserve(raw_vertices.size());

    for (const auto& v : raw_vertices) {
        auto [it, inserted] = vertex_map.emplace(v, static_cast<std::uint32_t>(unique_vertices.size()));
        if (inserted) {
            unique_vertices.push_back(v);
        }
        indices.push_back(it->second);
    }

    auto ml = build_meshlets(unique_vertices, indices);

    return {
        .material_name = material_name,
        .vertices = std::move(unique_vertices),
        .indices = std::move(indices),
        .meshlets = std::move(ml)
    };
}

auto gse::asset_compiler<gse::model>::build_meshlets(
    const std::vector<vertex>& vertices,
    const std::vector<std::uint32_t>& indices
) -> meshlet_data {
    constexpr std::uint32_t max_vertices = 64;
    constexpr std::uint32_t max_triangles = 124;

    meshlet_data result;

    std::unordered_map<std::uint32_t, std::uint8_t> local_vertex_map;
    std::vector<std::uint32_t> current_vertices;
    std::vector<std::uint8_t> current_triangles;

    auto finalize_meshlet = [&] {
        if (current_triangles.empty()) return;

        meshlet_descriptor desc{
            .vertex_offset = static_cast<std::uint32_t>(result.vertex_indices.size()),
            .triangle_offset = static_cast<std::uint32_t>(result.triangles.size()),
            .vertex_count = static_cast<std::uint32_t>(current_vertices.size()),
            .triangle_count = static_cast<std::uint32_t>(current_triangles.size() / 3)
        };

        auto bounds = compute_meshlet_bounds(
            vertices, desc,
            current_vertices,
            current_triangles
        );

        result.vertex_indices.insert(result.vertex_indices.end(), current_vertices.begin(), current_vertices.end());
        result.triangles.insert(result.triangles.end(), current_triangles.begin(), current_triangles.end());
        result.descriptors.push_back(desc);
        result.bounds.push_back(bounds);

        local_vertex_map.clear();
        current_vertices.clear();
        current_triangles.clear();
    };

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        std::uint32_t tri_indices[3] = { indices[i], indices[i + 1], indices[i + 2] };

        std::uint32_t new_vertex_count = 0;
        for (auto idx : tri_indices) {
            if (!local_vertex_map.contains(idx)) {
                ++new_vertex_count;
            }
        }

        if (current_vertices.size() + new_vertex_count > max_vertices ||
            current_triangles.size() / 3 >= max_triangles) {
            finalize_meshlet();
        }

        std::uint8_t local_tri[3];
        for (int j = 0; j < 3; ++j) {
            auto [it, inserted] = local_vertex_map.emplace(tri_indices[j], static_cast<std::uint8_t>(current_vertices.size()));
            if (inserted) {
                current_vertices.push_back(tri_indices[j]);
            }
            local_tri[j] = it->second;
        }

        current_triangles.push_back(local_tri[0]);
        current_triangles.push_back(local_tri[1]);
        current_triangles.push_back(local_tri[2]);
    }

    finalize_meshlet();

    while (result.triangles.size() % 4 != 0) {
        result.triangles.push_back(0);
    }

    return result;
}

auto gse::asset_compiler<gse::model>::compute_meshlet_bounds(
    const std::vector<vertex>& vertices,
    const meshlet_descriptor& desc,
    const std::span<const std::uint32_t> meshlet_vertex_indices,
    const std::span<const std::uint8_t> meshlet_triangles
) -> meshlet_bounds {
    vec3<displacement> centroid{};
    for (std::uint32_t i = 0; i < desc.vertex_count; ++i) {
        centroid += vertices[meshlet_vertex_indices[i]].position;
    }
    centroid /= static_cast<float>(desc.vertex_count);

    length max_dist{};
    for (std::uint32_t i = 0; i < desc.vertex_count; ++i) {
        const vec3<displacement> d = vertices[meshlet_vertex_indices[i]].position - centroid;
        max_dist = std::max<length>(max_dist, magnitude(d));
    }

    vec3f avg_normal(0.f);
    for (std::uint32_t t = 0; t < desc.triangle_count; ++t) {
        const auto i0 = meshlet_vertex_indices[meshlet_triangles[t * 3 + 0]];
        const auto i1 = meshlet_vertex_indices[meshlet_triangles[t * 3 + 1]];
        const auto i2 = meshlet_vertex_indices[meshlet_triangles[t * 3 + 2]];
        avg_normal += vertices[i0].normal + vertices[i1].normal + vertices[i2].normal;
    }

    const float normal_len = magnitude(avg_normal);
    vec3f cone_axis = normal_len > 1e-6f ? avg_normal / normal_len : vec3f(0.f, 1.f, 0.f);

    float min_dot = 1.f;
    for (std::uint32_t t = 0; t < desc.triangle_count; ++t) {
        const auto i0 = meshlet_vertex_indices[meshlet_triangles[t * 3 + 0]];
        const auto i1 = meshlet_vertex_indices[meshlet_triangles[t * 3 + 1]];
        const auto i2 = meshlet_vertex_indices[meshlet_triangles[t * 3 + 2]];

        const vec3<displacement> edge1 = vertices[i1].position - vertices[i0].position;
        const vec3<displacement> edge2 = vertices[i2].position - vertices[i0].position;
        const vec3f face_normal = normalize(cross(edge1, edge2));
        if (magnitude(face_normal) > 0.5f) {
            min_dot = std::min(min_dot, dot(face_normal, cone_axis));
        }
    }

    const float cone_cutoff = min_dot < 0.f ? -1.f : min_dot;

    return {
        .center = centroid,
        .radius = max_dist,
        .cone_axis = cone_axis,
        .cone_cutoff = cone_cutoff
    };
}
