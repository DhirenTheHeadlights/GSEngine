export module gse.graphics:mesh;

import std;

import :material;

import gse.platform;
import gse.math;

export namespace gse {
    struct vertex {
        vec3<length> position;
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
        vec3<length> center;
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

        auto initialize(vulkan::config& config) -> void;

        auto bind(vk::CommandBuffer command_buffer) const -> void;
        auto draw(vk::CommandBuffer command_buffer) const -> void;
        auto draw_instanced(vk::CommandBuffer command_buffer, std::uint32_t instance_count, std::uint32_t first_instance = 0) const -> void;

        auto center_of_mass() const -> vec3<length>;
        auto material() const -> const resource::handle<material>&;
        auto indices() const -> const std::vector<std::uint32_t>&;
        auto aabb() const -> std::pair<vec3<length>, vec3<length>>;

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

    auto generate_bounding_box_mesh(vec3<length> upper, vec3<length> lower) -> mesh_data;
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

auto gse::mesh::initialize(vulkan::config& config) -> void {
    const vk::DeviceSize vertex_buffer_size = sizeof(vertex) * m_vertices.size();
    const vk::DeviceSize index_buffer_size = sizeof(std::uint32_t) * m_indices.size();

    constexpr auto storage_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

    m_vertex_buffer = config.allocator().create_buffer({
        .size = vertex_buffer_size,
        .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
    });

    m_index_buffer = config.allocator().create_buffer({
        .size = index_buffer_size,
        .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
    });

    m_vertex_storage_buffer = config.allocator().create_buffer({
        .size = vertex_buffer_size,
        .usage = storage_usage
    });

    const bool has_ml = has_meshlets();

    if (has_ml) {
        m_meshlet_descriptor_buffer = config.allocator().create_buffer({
            .size = sizeof(meshlet_descriptor) * m_meshlets.descriptors.size(),
            .usage = storage_usage
        });

        m_meshlet_vertex_buffer = config.allocator().create_buffer({
            .size = sizeof(std::uint32_t) * m_meshlets.vertex_indices.size(),
            .usage = storage_usage
        });

        const auto tri_size = (m_meshlets.triangles.size() + 3) & ~std::size_t(3);
        m_meshlet_triangle_buffer = config.allocator().create_buffer({
            .size = tri_size,
            .usage = storage_usage
        });

        m_meshlet_bounds_buffer = config.allocator().create_buffer({
            .size = sizeof(meshlet_bounds) * m_meshlets.bounds.size(),
            .usage = storage_usage
        });
    }

    config.add_transient_work(
        [this, &config, vertex_buffer_size, index_buffer_size, has_ml](const vk::raii::CommandBuffer& command_buffer) -> std::vector<vulkan::buffer_resource> {
            std::vector<vulkan::buffer_resource> transient;

            auto stage_copy = [&](vk::Buffer dst, const void* data, vk::DeviceSize size) {
                auto staging = config.allocator().create_buffer({
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

auto gse::mesh::center_of_mass() const -> vec3<length> {
    using length_d = length_t<double>;
    using volume_d = volume_t<double>;
    using vec3ld = vec3<length_d>;

    const vec3ld reference_point{};

    volume_d total_volume{};
    vec3ld moment;

    assert(m_indices.size() % 3 == 0, std::source_location::current(), "m_indices count is not a multiple of 3. Ensure that each face is defined by exactly three m_indices.");

    for (size_t i = 0; i < m_indices.size(); i += 3) {
        const unsigned int idx0 = m_indices[i];
        const unsigned int idx1 = m_indices[i + 1];
        const unsigned int idx2 = m_indices[i + 2];

        assert(idx0 < m_vertices.size() && idx1 < m_vertices.size() && idx2 < m_vertices.size(), std::source_location::current(), "Index out of range while accessing m_vertices.");

        const vec3ld v0 = { length_d(m_vertices[idx0].position.x()), length_d(m_vertices[idx0].position.y()), length_d(m_vertices[idx0].position.z()) };
        const vec3ld v1 = { length_d(m_vertices[idx1].position.x()), length_d(m_vertices[idx1].position.y()), length_d(m_vertices[idx1].position.z()) };
        const vec3ld v2 = { length_d(m_vertices[idx2].position.x()), length_d(m_vertices[idx2].position.y()), length_d(m_vertices[idx2].position.z()) };

        const vec3ld a = v0 - reference_point;
        const vec3ld b = v1 - reference_point;
        const vec3ld c = v2 - reference_point;

        const volume_d volume = abs(dot(a, cross(b, c))) / 6.0;
        const vec3ld tetra_com = (v0 + v1 + v2 + reference_point) / 4.0;

        total_volume = total_volume + volume;
        moment += tetra_com * gse::internal::to_storage(volume);
    }

    assert(total_volume != volume_d{}, std::source_location::current(), "Total volume is zero. Check if the mesh is closed and correctly oriented.");

    const auto result = moment / gse::internal::to_storage(total_volume);
    return { length(result.x()), length(result.y()), length(result.z()) };
}

auto gse::mesh::material() const -> const resource::handle<gse::material>& {
    return m_material;
}

auto gse::mesh::indices() const -> const std::vector<std::uint32_t>& {
    return m_indices;
}

auto gse::mesh::aabb() const -> std::pair<vec3<length>, vec3<length>> {
    if (m_vertices.empty()) {
        return {};
    }

    vec3<length> min_point = m_vertices[0].position;
    vec3<length> max_point = m_vertices[0].position;

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

auto gse::generate_bounding_box_mesh(const vec3<length> upper, const vec3<length> lower) -> mesh_data {
    auto create_vertex = [](const vec3<length>& position) -> vertex {
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
