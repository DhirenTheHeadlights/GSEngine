export module gse.graphics:mesh;

import std;

import :material;

import gse.platform;
import gse.physics.math;

export namespace gse {
    struct vertex {
        vec3<length> position;
        unitless::vec3 normal;
        unitless::vec2 tex_coords;
    };

    struct mesh_data {
        std::vector<vertex> vertices;
        std::vector<std::uint32_t> indices;
        resource::handle<material> material;
    };

    class mesh final : non_copyable {
    public:
        explicit mesh(mesh_data&& data) : m_vertices(std::move(data.vertices)), m_indices(std::move(data.indices)), m_material(data.material) {}
        mesh(std::vector<vertex> vertices, std::vector<std::uint32_t> indices, const resource::handle<material>& material = {}) : m_vertices(std::move(vertices)), m_indices(std::move(indices)), m_material(material) {}

        mesh(mesh&& other) noexcept;

        auto initialize(vulkan::config& config) -> void;

        auto bind(vk::CommandBuffer command_buffer) const -> void;
        auto draw(vk::CommandBuffer command_buffer) const -> void;

        auto center_of_mass() const -> vec3<length>;
        auto material() const -> const resource::handle<material>&;
    private:
        vulkan::persistent_allocator::buffer_resource m_vertex_buffer;
        vulkan::persistent_allocator::buffer_resource m_index_buffer;

        std::vector<vertex> m_vertices;
        std::vector<std::uint32_t> m_indices;
        resource::handle<gse::material> m_material;
    };

    auto generate_bounding_box_mesh(vec3<length> upper, vec3<length> lower) -> mesh_data;
}

gse::mesh::mesh(mesh&& other) noexcept
    : m_vertex_buffer(std::move(other.m_vertex_buffer)),
    m_index_buffer(std::move(other.m_index_buffer)),
    m_vertices(std::move(other.m_vertices)),
    m_indices(std::move(other.m_indices)),
    m_material(std::move(other.m_material)) {
    other.m_vertex_buffer = {};
    other.m_index_buffer = {};
}

auto gse::mesh::initialize(vulkan::config& config) -> void {
    const vk::DeviceSize vertex_buffer_size = sizeof(vertex) * m_vertices.size();
    const vk::DeviceSize index_buffer_size = sizeof(std::uint32_t) * m_indices.size();

    const vk::BufferCreateInfo vertex_final_info{
        .size = vertex_buffer_size,
        .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
    };
    this->m_vertex_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_config(),
        vertex_final_info
    );

    const vk::BufferCreateInfo index_final_info{
        .size = index_buffer_size,
        .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
    };
    this->m_index_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_config(),
        index_final_info
    );

    config.add_transient_work(
        [&](const vk::raii::CommandBuffer& command_buffer) -> std::vector<vulkan::persistent_allocator::buffer_resource> {
            auto vertex_staging = vulkan::persistent_allocator::create_buffer(
                config.device_config(),
                vk::BufferCreateInfo{
                    .size = vertex_buffer_size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc
                },
                m_vertices.data()
            );

            auto index_staging = vulkan::persistent_allocator::create_buffer(
                config.device_config(),
                vk::BufferCreateInfo{
                    .size = index_buffer_size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc
                },
                m_indices.data()
            );

            const vk::BufferCopy vertex_copy_region(0, 0, vertex_buffer_size);
            command_buffer.copyBuffer(*vertex_staging.buffer, *this->m_vertex_buffer.buffer, vertex_copy_region);

            const vk::BufferCopy index_copy_region(0, 0, index_buffer_size);
            command_buffer.copyBuffer(*index_staging.buffer, *this->m_index_buffer.buffer, index_copy_region);

            std::vector<vulkan::persistent_allocator::buffer_resource> transient_resources;
            transient_resources.push_back(std::move(vertex_staging));
            transient_resources.push_back(std::move(index_staging));
            return transient_resources;
        }
    );
}

auto gse::mesh::bind(const vk::CommandBuffer command_buffer) const -> void {
    if (!*m_vertex_buffer.buffer || !*m_index_buffer.buffer) {
        return;
    }

    command_buffer.bindVertexBuffers(0, { *m_vertex_buffer.buffer }, { 0 });
    command_buffer.bindIndexBuffer(*m_index_buffer.buffer, 0, vk::IndexType::eUint32);
}

auto gse::mesh::draw(const vk::CommandBuffer command_buffer) const -> void {
    command_buffer.drawIndexed(static_cast<std::uint32_t>(m_indices.size()), 1, 0, 0, 0);
}

auto gse::mesh::center_of_mass() const -> vec3<length> {
    constexpr unitless::vec3d reference_point(0.f);

    double total_volume = 0.f;
    unitless::vec3 moment(0.f);

    assert(m_indices.size() % 3 == 0, "m_indices count is not a multiple of 3. Ensure that each face is defined by exactly three m_indices.");

    for (size_t i = 0; i < m_indices.size(); i += 3) {
        const unsigned int idx0 = m_indices[i];
        const unsigned int idx1 = m_indices[i + 1];
        const unsigned int idx2 = m_indices[i + 2];

        assert(idx0 < m_vertices.size() && idx1 < m_vertices.size() && idx2 < m_vertices.size(), "Index out of range while accessing m_vertices.");

        const unitless::vec3d v0(m_vertices[idx0].position.as<meters>());
        const unitless::vec3d v1(m_vertices[idx1].position.as<meters>());
        const unitless::vec3d v2(m_vertices[idx2].position.as<meters>());

        unitless::vec3d a = v0 - reference_point;
        unitless::vec3d b = v1 - reference_point;
        unitless::vec3d c = v2 - reference_point;

        auto volume = std::abs(dot(a, cross(b, c)) / 6.0);
        unitless::vec3d tetra_com = (v0 + v1 + v2 + reference_point) / 4.0;

        total_volume += volume;
        moment += unitless::vec3(tetra_com * volume);
    }

    assert(total_volume != 0.0, "Total volume is zero. Check if the mesh is closed and correctly oriented.");

    return moment / static_cast<float>(total_volume);
}

auto gse::mesh::material() const -> const resource::handle<gse::material>& {
    return m_material;
}

auto gse::generate_bounding_box_mesh(const vec3<length> upper, const vec3<length> lower) -> mesh_data {
    auto create_vertex = [](
        const vec3<length>& position
        ) -> vertex {
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
