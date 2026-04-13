export module gse.graphics:skinned_mesh;

import std;

import :material;

import gse.platform;
import gse.math;
import gse.assert;

export namespace gse {
    struct skinned_vertex {
        vec3<displacement> position;
        vec3f normal;
        vec2f tex_coords;
        std::array<std::uint32_t, 4> bone_indices;
        vec4f bone_weights;
    };

    struct skinned_mesh_data {
        std::vector<skinned_vertex> vertices;
        std::vector<std::uint32_t> indices;
        gse::material material;
    };

    class skinned_mesh final : non_copyable {
    public:
        explicit skinned_mesh(skinned_mesh_data&& data) : m_vertices(std::move(data.vertices)), m_indices(std::move(data.indices)), m_material(data.material) {}

        skinned_mesh(skinned_mesh&& other) noexcept;

        auto initialize(gpu::resource_manager& ctx) -> void;

        auto vertex_gpu_buffer(this const skinned_mesh& self) -> const gpu::buffer& { return self.m_vertex_buffer; }
        auto index_gpu_buffer(this const skinned_mesh& self) -> const gpu::buffer& { return self.m_index_buffer; }

        auto center_of_mass() const -> vec3<displacement>;
        auto material() const -> const gse::material&;
        auto indices() const -> const std::vector<std::uint32_t>&;
        auto aabb() const -> std::pair<vec3<displacement>, vec3<displacement>>;
    private:
        gpu::buffer m_vertex_buffer;
        gpu::buffer m_index_buffer;

        std::vector<skinned_vertex> m_vertices;
        std::vector<std::uint32_t> m_indices;
        gse::material m_material;
    };
}

gse::skinned_mesh::skinned_mesh(skinned_mesh&& other) noexcept
    : m_vertex_buffer(std::move(other.m_vertex_buffer)),
    m_index_buffer(std::move(other.m_index_buffer)),
    m_vertices(std::move(other.m_vertices)),
    m_indices(std::move(other.m_indices)),
    m_material(std::move(other.m_material)) {
    other.m_vertex_buffer = {};
    other.m_index_buffer = {};
}

auto gse::skinned_mesh::initialize(gpu::resource_manager& ctx) -> void {
    if (m_vertices.empty() || m_indices.empty()) {
        return;
    }

    const std::size_t vertex_buffer_size = sizeof(skinned_vertex) * m_vertices.size();
    const std::size_t index_buffer_size = sizeof(std::uint32_t) * m_indices.size();

    m_vertex_buffer = gpu::create_buffer(ctx.device_ref(), {
        .size = vertex_buffer_size,
        .usage = gpu::buffer_flag::vertex | gpu::buffer_flag::transfer_dst
    });

    m_index_buffer = gpu::create_buffer(ctx.device_ref(), {
        .size = index_buffer_size,
        .usage = gpu::buffer_flag::index | gpu::buffer_flag::transfer_dst
    });

    gpu::upload_to_buffers(ctx.device_ref(), std::array{
        gpu::buffer_upload{ &m_vertex_buffer, m_vertices.data(), vertex_buffer_size },
        gpu::buffer_upload{ &m_index_buffer, m_indices.data(), index_buffer_size }
    });
}

auto gse::skinned_mesh::center_of_mass() const -> vec3<displacement> {
    using length_d = length_t<double>;
    using volume_d = volume_t<double>;
    using vec3_ld = vec3<length_d>;

    const vec3_ld reference_point{};

    volume_d total_volume{};
    decltype(vec3_ld{} * volume_d{}) moment{};

    assert(m_indices.size() % 3 == 0, std::source_location::current(), "m_indices count is not a multiple of 3.");

    for (size_t i = 0; i < m_indices.size(); i += 3) {
        const unsigned int idx0 = m_indices[i];
        const unsigned int idx1 = m_indices[i + 1];
        const unsigned int idx2 = m_indices[i + 2];

        assert(idx0 < m_vertices.size() && idx1 < m_vertices.size() && idx2 < m_vertices.size(), std::source_location::current(), "Index out of range.");

        const vec3_ld v0 = { length_d(m_vertices[idx0].position.x()), length_d(m_vertices[idx0].position.y()), length_d(m_vertices[idx0].position.z()) };
        const vec3_ld v1 = { length_d(m_vertices[idx1].position.x()), length_d(m_vertices[idx1].position.y()), length_d(m_vertices[idx1].position.z()) };
        const vec3_ld v2 = { length_d(m_vertices[idx2].position.x()), length_d(m_vertices[idx2].position.y()), length_d(m_vertices[idx2].position.z()) };

        const vec3_ld a = v0 - reference_point;
        const vec3_ld b = v1 - reference_point;
        const vec3_ld c = v2 - reference_point;

        const volume_d volume = abs(dot(a, cross(b, c))) / 6.0;
        const vec3_ld tetra_com = (v0 + v1 + v2 + reference_point) / 4.0;

        total_volume = total_volume + volume;
        moment += tetra_com * volume;
    }

    assert(total_volume != volume_d{}, std::source_location::current(), "Total volume is zero.");

    return vec3<displacement>(moment / total_volume);
}

auto gse::skinned_mesh::material() const -> const gse::material& {
    return m_material;
}

auto gse::skinned_mesh::indices() const -> const std::vector<std::uint32_t>& {
    return m_indices;
}

auto gse::skinned_mesh::aabb() const -> std::pair<vec3<displacement>, vec3<displacement>> {
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
