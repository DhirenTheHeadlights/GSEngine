export module gse.graphics:model;

import std;

import :mesh;
import :rendering_context;
import :material;

import gse.utility;
import gse.platform;
import gse.physics.math;
import gse.assert;

export namespace gse {
    class model : public identifiable {
    public:
        class handle {
        public:
            explicit handle(model& model) : m_render_queue_entries(model.m_meshes.size()), m_owner(&model) {
                for (size_t i = 0; i < model.m_meshes.size(); ++i) {
                    m_render_queue_entries[i] = render_queue_entry(
                        &model.m_meshes[i],
                        mat4(1.0f),
                        unitless::vec3(1.0f)
                    );
                }
            }

            auto set_position(const vec3<length>& position) -> void {
                for (auto& entry : m_render_queue_entries) {
                    entry.model_matrix = translate(mat4(1.0f), position);
                }
            }

            auto set_rotation(const vec3<angle>& rotation) -> void {
                for (auto& entry : m_render_queue_entries) {
                    entry.model_matrix = rotate(entry.model_matrix, axis::x, rotation.x);
                    entry.model_matrix = rotate(entry.model_matrix, axis::y, rotation.y);
                    entry.model_matrix = rotate(entry.model_matrix, axis::z, rotation.z);
                }
            }

            auto render_queue_entries() const -> std::span<const render_queue_entry> {
                return m_render_queue_entries;
            }

            auto id() const -> gse::id {
                return m_model_id;
            }

            auto owner() const -> const model& {
                return *m_owner;
			}
        private:
            std::vector<render_queue_entry> m_render_queue_entries;
            gse::id m_model_id;
			model* m_owner = nullptr;
        };

        explicit model(const std::filesystem::path& model_path) : identifiable(model_path.stem().string()), m_model_path(model_path) {}

        auto load(renderer::context& context) -> void;
        auto unload() -> void;

        auto meshes() const -> std::span<const mesh>;
    private:
        std::vector<mesh> m_meshes;
        vec3<length>  m_center_of_mass{};
        std::filesystem::path m_model_path;
    };
}

auto gse::model::load(renderer::context& context) -> void {
    std::ifstream model_file(m_model_path);
    assert(model_file.is_open(), "Failed to open model file.");

    auto split = [](
        const std::string& str, 
        const char delimiter = ' '
        ) -> std::vector<std::string> {
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

    std::string file_line;
    bool push_back_mesh = false;

    std::vector<raw3f> pre_load_vertices;
    std::vector<raw2f> pre_load_texcoords;
    std::vector<raw3f> pre_load_normals;
    std::vector<vertex> final_vertices;

    gse::id current_material;
    std::filesystem::path mtl_dir;

    while (std::getline(model_file, file_line)) {
        const auto split_line = split(file_line, ' ');

        if (file_line.rfind("mtllib ", 0) == 0) {
            mtl_dir = m_model_path.parent_path();
            auto mtl_file = mtl_dir / split_line[1];

            current_material = context.queue<material>(mtl_file);
            continue;
        }
        if (file_line.rfind("usemtl ", 0) == 0) {
            continue;
        }
        if (file_line.rfind("v ", 0) == 0) {
            if (push_back_mesh) {
                std::vector<std::uint32_t> final_indices(final_vertices.size());
                for (size_t i = 0; i < final_vertices.size(); ++i) {
                    final_indices[i] = static_cast<std::uint32_t>(i);
                }
                m_meshes.emplace_back(
                    final_vertices,
                    final_indices,
                    current_material
                );
                push_back_mesh = false;
                final_vertices.clear();
            }

            pre_load_vertices.push_back({
                std::stof(split_line[1]),
                std::stof(split_line[2]),
                std::stof(split_line[3])
                });
        }
        else if (file_line.rfind("vt ", 0) == 0) {
            pre_load_texcoords.push_back({
                std::stof(split_line[1]),
                std::stof(split_line[2])
                });
        }
        else if (file_line.rfind("vn ", 0) == 0) {
            pre_load_normals.push_back({
                std::stof(split_line[1]),
                std::stof(split_line[2]),
                std::stof(split_line[3])
                });
        }
        else if (file_line.rfind("f ", 0) == 0) {
            push_back_mesh = true;

            const auto process = [&](const std::string& token) {
	            if (const auto vm = split(token, '/'); vm.size() == 3) {
                    final_vertices.emplace_back(
                        pre_load_vertices[std::stoi(vm[0]) - 1],
                        pre_load_normals[std::stoi(vm[2]) - 1],
                        pre_load_texcoords[std::stoi(vm[1]) - 1]
                    );
                }
                else if (!pre_load_normals.empty()) {
                    final_vertices.emplace_back(
                        pre_load_vertices[std::stoi(vm[0]) - 1],
                        pre_load_normals[std::stoi(vm[1]) - 1],
                        raw2f()
                    );
                }
                else if (!pre_load_texcoords.empty()) {
                    final_vertices.emplace_back(
                        pre_load_vertices[std::stoi(vm[0]) - 1],
                        raw3f(),
                        pre_load_texcoords[std::stoi(vm[1]) - 1]
                    );
                }
                };

            if (split_line.size() - 1 == 3) {
                for (int i = 1; i <= 3; ++i) process(split_line[i]);
            }
            else if (split_line.size() - 1 == 4) {
                process(split_line[1]); process(split_line[2]); process(split_line[3]);
                process(split_line[1]); process(split_line[3]); process(split_line[4]);
            }
        }
    }

    if (push_back_mesh) {
        std::vector<std::uint32_t> final_indices(final_vertices.size());
        for (size_t i = 0; i < final_vertices.size(); ++i) {
            final_indices[i] = static_cast<std::uint32_t>(i);
        }
        m_meshes.emplace_back(
            final_vertices,
            final_indices,
            current_material
        );
    }

    for (auto& mesh : m_meshes) {
        mesh.initialize(context.config());
    }
}

auto gse::model::unload() -> void {
    m_meshes.clear();
}

auto gse::model::meshes() const -> std::span<const mesh> {
    return m_meshes;
}
