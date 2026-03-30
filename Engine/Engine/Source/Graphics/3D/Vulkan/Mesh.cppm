export module gse.graphics:mesh;

import std;

import :material;

import gse.platform;
import gse.math;

export namespace gse {
    struct vertex {
        vec3<displacement> position;
        vec3f normal;
        vec2f tex_coords;

        auto operator==(const vertex&) const -> bool = default;
    };

    struct meshlet_descriptor {
        std::uint32_t vertex_offset;
        std::uint32_t triangle_offset;
        std::uint32_t vertex_count;
        std::uint32_t triangle_count;
    };

    struct meshlet_bounds {
        vec3<displacement> center;
        length radius;
        vec3f cone_axis;
        float cone_cutoff;
    };

    struct meshlet_data {
        std::vector<meshlet_descriptor> descriptors;
        std::vector<std::uint32_t> vertex_indices;
        std::vector<std::uint8_t> triangles;
        std::vector<meshlet_bounds> bounds;
    };

    struct mesh_data {
        std::vector<vertex> vertices;
        std::vector<std::uint32_t> indices;
        resource::handle<material> material;
        meshlet_data meshlets;
    };

    class mesh final : non_copyable {
    public:
        explicit mesh(mesh_data&& data);
        mesh(std::vector<vertex> vertices, std::vector<std::uint32_t> indices, const resource::handle<material>& material = {}) : m_vertices(std::move(vertices)), m_indices(std::move(indices)), m_material(material) {}

        mesh(mesh&& other) noexcept;

        auto initialize(gpu::context& ctx) -> void;

        auto bind(vk::CommandBuffer command_buffer) const -> void;
        auto draw(vk::CommandBuffer command_buffer) const -> void;
        auto draw_instanced(vk::CommandBuffer command_buffer, std::uint32_t instance_count, std::uint32_t first_instance = 0) const -> void;

        auto center_of_mass() const -> vec3<displacement>;
        auto material() const -> const resource::handle<material>&;
        auto indices() const -> const std::vector<std::uint32_t>&;
        auto aabb() const -> std::pair<vec3<displacement>, vec3<displacement>>;

        auto vertex_buffer() const -> vk::Buffer { return m_vertex_buffer.buffer; }
        auto index_buffer() const -> vk::Buffer { return m_index_buffer.buffer; }

        auto has_meshlets() const -> bool { return !m_meshlets.descriptors.empty(); }
        auto meshlet_count() const -> std::uint32_t { return static_cast<std::uint32_t>(m_meshlets.descriptors.size()); }
        auto meshlet_descriptor_buffer() const -> vk::Buffer { return m_meshlet_descriptor_buffer.buffer; }
        auto meshlet_vertex_buffer() const -> vk::Buffer { return m_meshlet_vertex_buffer.buffer; }
        auto meshlet_triangle_buffer() const -> vk::Buffer { return m_meshlet_triangle_buffer.buffer; }
        auto meshlet_bounds_buffer() const -> vk::Buffer { return m_meshlet_bounds_buffer.buffer; }
        auto vertex_storage_buffer() const -> vk::Buffer { return m_vertex_storage_buffer.buffer; }
    private:
        vulkan::buffer_resource m_vertex_buffer;
        vulkan::buffer_resource m_index_buffer;
        vulkan::buffer_resource m_vertex_storage_buffer;
        vulkan::buffer_resource m_meshlet_descriptor_buffer;
        vulkan::buffer_resource m_meshlet_vertex_buffer;
        vulkan::buffer_resource m_meshlet_triangle_buffer;
        vulkan::buffer_resource m_meshlet_bounds_buffer;

        std::vector<vertex> m_vertices;
        std::vector<std::uint32_t> m_indices;
        resource::handle<gse::material> m_material;
        meshlet_data m_meshlets;
    };

    auto generate_bounding_box_mesh(
        vec3<displacement> upper, 
        vec3<displacement> lower
    ) -> mesh_data;

    auto build_runtime_meshlets(
        const std::vector<vertex>& vertices, 
        const std::vector<std::uint32_t>& indices
    ) -> meshlet_data;
}

gse::mesh::mesh(mesh_data&& data)
    : m_vertices(std::move(data.vertices)),
    m_indices(std::move(data.indices)),
    m_material(data.material),
    m_meshlets(std::move(data.meshlets)) {}

gse::mesh::mesh(mesh&& other) noexcept
    : m_vertex_buffer(std::move(other.m_vertex_buffer)),
    m_index_buffer(std::move(other.m_index_buffer)),
    m_vertex_storage_buffer(std::move(other.m_vertex_storage_buffer)),
    m_meshlet_descriptor_buffer(std::move(other.m_meshlet_descriptor_buffer)),
    m_meshlet_vertex_buffer(std::move(other.m_meshlet_vertex_buffer)),
    m_meshlet_triangle_buffer(std::move(other.m_meshlet_triangle_buffer)),
    m_meshlet_bounds_buffer(std::move(other.m_meshlet_bounds_buffer)),
    m_vertices(std::move(other.m_vertices)),
    m_indices(std::move(other.m_indices)),
    m_material(std::move(other.m_material)),
    m_meshlets(std::move(other.m_meshlets)) {
    other.m_vertex_buffer = {};
    other.m_index_buffer = {};
    other.m_vertex_storage_buffer = {};
    other.m_meshlet_descriptor_buffer = {};
    other.m_meshlet_vertex_buffer = {};
    other.m_meshlet_triangle_buffer = {};
    other.m_meshlet_bounds_buffer = {};
}

auto gse::mesh::initialize(gpu::context& ctx) -> void {
    if (!has_meshlets() && !m_vertices.empty() && !m_indices.empty()) {
        m_meshlets = build_runtime_meshlets(m_vertices, m_indices);
    }

    const vk::DeviceSize vertex_buffer_size = sizeof(vertex) * m_vertices.size();
    const vk::DeviceSize index_buffer_size = sizeof(std::uint32_t) * m_indices.size();

    constexpr auto storage_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

    m_vertex_buffer = ctx.allocator().create_buffer({
        .size = vertex_buffer_size,
        .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
    });

    m_index_buffer = ctx.allocator().create_buffer({
        .size = index_buffer_size,
        .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
    });

    m_vertex_storage_buffer = ctx.allocator().create_buffer({
        .size = vertex_buffer_size,
        .usage = storage_usage
    });

    const bool has_ml = has_meshlets();

    if (has_ml) {
        m_meshlet_descriptor_buffer = ctx.allocator().create_buffer({
            .size = sizeof(meshlet_descriptor) * m_meshlets.descriptors.size(),
            .usage = storage_usage
        });

        m_meshlet_vertex_buffer = ctx.allocator().create_buffer({
            .size = sizeof(std::uint32_t) * m_meshlets.vertex_indices.size(),
            .usage = storage_usage
        });

        const auto tri_size = (m_meshlets.triangles.size() + 3) & ~std::size_t(3);
        m_meshlet_triangle_buffer = ctx.allocator().create_buffer({
            .size = tri_size,
            .usage = storage_usage
        });

        m_meshlet_bounds_buffer = ctx.allocator().create_buffer({
            .size = sizeof(meshlet_bounds) * m_meshlets.bounds.size(),
            .usage = storage_usage
        });
    }

    ctx.add_transient_work(
        [this, &ctx, vertex_buffer_size, index_buffer_size, has_ml](const vk::raii::CommandBuffer& command_buffer) -> std::vector<vulkan::buffer_resource> {
            std::vector<vulkan::buffer_resource> transient;

            auto stage_copy = [&](vk::Buffer dst, const void* data, vk::DeviceSize size) {
                auto staging = ctx.allocator().create_buffer({
                    .size = size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc
                }, data);
                command_buffer.copyBuffer(staging.buffer, dst, vk::BufferCopy(0, 0, size));
                transient.push_back(std::move(staging));
            };

            stage_copy(m_vertex_buffer.buffer, m_vertices.data(), vertex_buffer_size);
            stage_copy(m_index_buffer.buffer, m_indices.data(), index_buffer_size);
            stage_copy(m_vertex_storage_buffer.buffer, m_vertices.data(), vertex_buffer_size);

            if (has_ml) {
                stage_copy(m_meshlet_descriptor_buffer.buffer, m_meshlets.descriptors.data(),
                    sizeof(meshlet_descriptor) * m_meshlets.descriptors.size());
                stage_copy(m_meshlet_vertex_buffer.buffer, m_meshlets.vertex_indices.data(),
                    sizeof(std::uint32_t) * m_meshlets.vertex_indices.size());
                const auto tri_size = (m_meshlets.triangles.size() + 3) & ~std::size_t(3);
                stage_copy(m_meshlet_triangle_buffer.buffer, m_meshlets.triangles.data(), tri_size);
                stage_copy(m_meshlet_bounds_buffer.buffer, m_meshlets.bounds.data(),
                    sizeof(meshlet_bounds) * m_meshlets.bounds.size());
            }

            return transient;
        }
    );
}

auto gse::mesh::bind(const vk::CommandBuffer command_buffer) const -> void {
    if (!m_vertex_buffer.buffer || !m_index_buffer.buffer) {
        return;
    }

    command_buffer.bindVertexBuffers(0, { m_vertex_buffer.buffer }, { 0 });
    command_buffer.bindIndexBuffer(m_index_buffer.buffer, 0, vk::IndexType::eUint32);
}

auto gse::mesh::draw(const vk::CommandBuffer command_buffer) const -> void {
    command_buffer.drawIndexed(static_cast<std::uint32_t>(m_indices.size()), 1, 0, 0, 0);
}

auto gse::mesh::draw_instanced(const vk::CommandBuffer command_buffer, const std::uint32_t instance_count, const std::uint32_t first_instance) const -> void {
    command_buffer.drawIndexed(static_cast<std::uint32_t>(m_indices.size()), instance_count, 0, 0, first_instance);
}

auto gse::mesh::center_of_mass() const -> vec3<displacement> {
    using length_d = length_t<double>;
    using volume_d = volume_t<double>;
    using vec3_ld = vec3<length_d>;

    constexpr vec3_ld reference_point{};

    volume_d total_volume{};
    decltype(vec3_ld{} * volume_d{}) moment{};

    assert(m_indices.size() % 3 == 0, std::source_location::current(), "m_indices count is not a multiple of 3. Ensure that each face is defined by exactly three m_indices.");

    for (size_t i = 0; i < m_indices.size(); i += 3) {
        const unsigned int idx0 = m_indices[i];
        const unsigned int idx1 = m_indices[i + 1];
        const unsigned int idx2 = m_indices[i + 2];

        assert(idx0 < m_vertices.size() && idx1 < m_vertices.size() && idx2 < m_vertices.size(), std::source_location::current(), "Index out of range while accessing m_vertices.");

        const vec3_ld v0 = { length_d(m_vertices[idx0].position.x()), length_d(m_vertices[idx0].position.y()), length_d(m_vertices[idx0].position.z()) };
        const vec3_ld v1 = { length_d(m_vertices[idx1].position.x()), length_d(m_vertices[idx1].position.y()), length_d(m_vertices[idx1].position.z()) };
        const vec3_ld v2 = { length_d(m_vertices[idx2].position.x()), length_d(m_vertices[idx2].position.y()), length_d(m_vertices[idx2].position.z()) };

        const vec3_ld a = v0 - reference_point;
        const vec3_ld b = v1 - reference_point;
        const vec3_ld c = v2 - reference_point;

        const auto volume = abs(dot(a, cross(b, c))) / 6.0;
        const vec3_ld tetra_com = (v0 + v1 + v2 + reference_point) / 4.0;

        total_volume = total_volume + volume;
        moment += tetra_com * volume;
    }

    assert(total_volume != volume_d{}, std::source_location::current(), "Total volume is zero. Check if the mesh is closed and correctly oriented.");

    return vec3<displacement>(moment / total_volume);
}

auto gse::mesh::material() const -> const resource::handle<gse::material>& {
    return m_material;
}

auto gse::mesh::indices() const -> const std::vector<std::uint32_t>& {
    return m_indices;
}

auto gse::mesh::aabb() const -> std::pair<vec3<displacement>, vec3<displacement>> {
    if (m_vertices.empty()) {
        return {};
    }

    vec3<displacement> min_point = m_vertices[0].position;
    vec3<displacement> max_point = m_vertices[0].position;

    for (const auto& vertex : m_vertices) {
        min_point.x() = std::min(min_point.x(), vertex.position.x());
        min_point.y() = std::min(min_point.y(), vertex.position.y());
        min_point.z() = std::min(min_point.z(), vertex.position.z());

        max_point.x() = std::max(max_point.x(), vertex.position.x());
        max_point.y() = std::max(max_point.y(), vertex.position.y());
        max_point.z() = std::max(max_point.z(), vertex.position.z());
    }

    return { min_point, max_point };
}

auto gse::generate_bounding_box_mesh(const vec3<displacement> upper, const vec3<displacement> lower) -> mesh_data {
    auto create_vertex = [](const vec3<displacement>& position) -> vertex {
		return {
			.position = position,
			.normal = { 0.0f, 0.0f, 0.0f },
			.tex_coords = { 0.0f, 0.0f }
		};
    };

    const std::vector vertices = {
        create_vertex({ lower.x(), lower.y(), lower.z() }),
        create_vertex({ upper.x(), lower.y(), lower.z() }),
        create_vertex({ upper.x(), upper.y(), lower.z() }),
        create_vertex({ lower.x(), upper.y(), lower.z() }),
        create_vertex({ lower.x(), lower.y(), upper.z() }),
        create_vertex({ upper.x(), lower.y(), upper.z() }),
        create_vertex({ upper.x(), upper.y(), upper.z() }),
        create_vertex({ lower.x(), upper.y(), upper.z() })
    };

    const std::vector<std::uint32_t> indices = {
        0, 1, 2, 2, 3, 0, // Front face
        4, 5, 6, 6, 7, 4, // Back face
        0, 4, 7, 7, 3, 0, // Left face
        1, 5, 6, 6, 2, 1, // Right face
        0, 1, 5, 5, 4, 0, // Bottom face
        3, 2, 6, 6, 7, 3  // Top face
    };

    return mesh_data{
        .vertices = vertices,
        .indices = indices
    };
}

auto gse::build_runtime_meshlets(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices) -> meshlet_data {
    constexpr std::uint32_t max_vertices = 64;
    constexpr std::uint32_t max_triangles = 124;

    meshlet_data result;

    std::unordered_map<std::uint32_t, std::uint8_t> local_vertex_map;
    std::vector<std::uint32_t> current_vertices;
    std::vector<std::uint8_t> current_triangles;

    auto compute_bounds = [&](const meshlet_descriptor& desc) -> meshlet_bounds {
        vec3<displacement> centroid{};
        for (std::uint32_t i = 0; i < desc.vertex_count; ++i) {
            centroid += vertices[current_vertices[i]].position;
        }
        centroid /= static_cast<float>(desc.vertex_count);

        length max_dist{};
        for (std::uint32_t i = 0; i < desc.vertex_count; ++i) {
            const vec3<displacement> d = vertices[current_vertices[i]].position - centroid;
            max_dist = std::max<displacement>(max_dist, magnitude(d));
        }

        vec3f avg_normal(0.f);
        for (std::uint32_t t = 0; t < desc.triangle_count; ++t) {
            const auto i0 = current_vertices[current_triangles[t * 3 + 0]];
            const auto i1 = current_vertices[current_triangles[t * 3 + 1]];
            const auto i2 = current_vertices[current_triangles[t * 3 + 2]];
            avg_normal += vertices[i0].normal + vertices[i1].normal + vertices[i2].normal;
        }

        const float normal_len = magnitude(avg_normal);
        const vec3f cone_axis = normal_len > 1e-6f ? avg_normal / normal_len : vec3f(0.f, 1.f, 0.f);

        float min_dot = 1.f;
        for (std::uint32_t t = 0; t < desc.triangle_count; ++t) {
            const auto i0 = current_vertices[current_triangles[t * 3 + 0]];
            const auto i1 = current_vertices[current_triangles[t * 3 + 1]];
            const auto i2 = current_vertices[current_triangles[t * 3 + 2]];

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
    };

    auto finalize_meshlet = [&] {
        if (current_triangles.empty()) {
            return;
        }

        const meshlet_descriptor desc{
            .vertex_offset = static_cast<std::uint32_t>(result.vertex_indices.size()),
            .triangle_offset = static_cast<std::uint32_t>(result.triangles.size()),
            .vertex_count = static_cast<std::uint32_t>(current_vertices.size()),
            .triangle_count = static_cast<std::uint32_t>(current_triangles.size() / 3)
        };

        const auto bounds = compute_bounds(desc);

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
        for (const auto idx : tri_indices) {
            if (!local_vertex_map.contains(idx)) {
                ++new_vertex_count;
            }
        }

        if (current_vertices.size() + new_vertex_count > max_vertices || current_triangles.size() / 3 >= max_triangles) {
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
