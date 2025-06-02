export module gse.graphics.mesh;

import std;
import vulkan_hpp;

import gse.graphics.material;
import gse.platform;
import gse.physics.bounding_box;
import gse.physics.math;

export namespace gse {
    struct vertex {
        vec::raw3f position;
        vec::raw3f normal;
        vec::raw2f tex_coords;
    };

    struct mesh;

    struct render_queue_entry {
		const mesh* mesh;
        mat4 model_matrix;
		unitless::vec3 color;
    };

	struct mesh_data {
		std::vector<vertex> vertices;
		std::vector<std::uint32_t> indices;
		material_handle material;
	};

    struct mesh {
		mesh(const mesh_data& data) : vertices(std::move(data.vertices)), indices(std::move(data.indices)), material(data.material) {}
		mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, material_handle material = {});

        mesh(const mesh&) = delete;
        auto operator=(const mesh&) -> mesh & = delete;
        mesh(mesh&& other) noexcept;
		auto operator=(mesh&& other) noexcept -> mesh& = delete;

        auto initialize(const vulkan::config& config) -> void;
        auto destroy(const vulkan::config::device_config config) -> void;

        auto bind(vk::CommandBuffer command_buffer) const -> void;
        auto draw(vk::CommandBuffer command_buffer) const -> void;

		vulkan::persistent_allocator::buffer_resource vertex_buffer;
        vulkan::persistent_allocator::buffer_resource index_buffer;

		vulkan::config::device_config device_config;

        std::vector<vertex> vertices;
        std::vector<std::uint32_t> indices;
		material_handle material;

		vec3<length> center_of_mass;
    };

    auto calculate_center_of_mass(const std::vector<std::uint32_t>& indices, const std::vector<vertex>& vertices) -> vec3<length>;
	auto generate_bounding_box_mesh(const axis_aligned_bounding_box& aabb) -> mesh;
}

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const material_handle material)
	: vertices(vertices), indices(indices), material(material) {}

gse::mesh::mesh(mesh&& other) noexcept
    : vertex_buffer(other.vertex_buffer), index_buffer(other.index_buffer),
    vertices(std::move(other.vertices)), indices(std::move(other.indices)), material(nullptr),
    center_of_mass(other.center_of_mass) {
    other.vertex_buffer = {};
    other.index_buffer = {};
}

auto gse::mesh::initialize(const vulkan::config& config) -> void {
    // --- TEMPORARY DEBUGGING VERSION ---
    // This version skips the staging buffer and creates host-visible buffers directly.
    // This is slower for rendering but excellent for debugging data uploads.

    if (vertices.empty() || indices.empty()) {
        // Don't try to create zero-sized buffers
        return;
    }

    // 1. Create and upload vertex buffer directly
    const vk::DeviceSize vertex_buffer_size = sizeof(vertex) * vertices.size();
    const vk::BufferCreateInfo vertex_buffer_info(
        {},
        vertex_buffer_size,
        vk::BufferUsageFlagBits::eVertexBuffer // No need for TransferDst
    );
    this->vertex_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_data,
        vertex_buffer_info,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        vertices.data() // Copy data directly using your fixed allocator
    );

    // 2. Create and upload index buffer directly
    const vk::DeviceSize index_buffer_size = sizeof(std::uint32_t) * indices.size();
    const vk::BufferCreateInfo index_buffer_info(
        {},
        index_buffer_size,
        vk::BufferUsageFlagBits::eIndexBuffer // No need for TransferDst
    );
    this->index_buffer = vulkan::persistent_allocator::create_buffer(
        config.device_data,
        index_buffer_info,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        indices.data() // Copy data directly using your fixed allocator
    );

    // No copy commands or staging buffers are needed for this test.

    this->center_of_mass = calculate_center_of_mass(indices, vertices);
}

auto gse::mesh::destroy(const vulkan::config::device_config config) -> void {
	free(config, vertex_buffer);
	free(config, index_buffer);
}

auto gse::mesh::bind(const vk::CommandBuffer command_buffer) const -> void {
    command_buffer.bindVertexBuffers(0, { vertex_buffer.buffer }, { 0 });
    command_buffer.bindIndexBuffer(index_buffer.buffer, 0, vk::IndexType::eUint32);
}

auto gse::mesh::draw(const vk::CommandBuffer command_buffer) const -> void {
    command_buffer.drawIndexed(static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
}

auto gse::calculate_center_of_mass(const std::vector<std::uint32_t>& indices, const std::vector<vertex>& vertices) -> vec3<length> {
    constexpr unitless::vec3d reference_point(0.f);

    double total_volume = 0.f;
    unitless::vec3 moment(0.f);

    assert(indices.size() % 3 == 0, "Indices count is not a multiple of 3. Ensure that each face is defined by exactly three indices.");

    for (size_t i = 0; i < indices.size(); i += 3) {
        const unsigned int idx0 = indices[i];
        const unsigned int idx1 = indices[i + 1];
        const unsigned int idx2 = indices[i + 2];

        assert(idx0 <= vertices.size() || idx1 <= vertices.size() || idx2 <= vertices.size(), "Index out of range while accessing vertices.");

        const unitless::vec3d v0(vertices[idx0].position);
        const unitless::vec3d v1(vertices[idx1].position);
        const unitless::vec3d v2(vertices[idx2].position);

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

auto gse::generate_bounding_box_mesh(const axis_aligned_bounding_box& aabb) -> mesh {
	const auto lower = aabb.lower_bound.as<units::meters>();
	const auto upper = aabb.upper_bound.as<units::meters>();

	auto create_vertex = [](const vec::raw3f& position) -> vertex {
		return vertex{ position, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f} };
		};

	const std::vector vertices = {
		create_vertex({ lower.x, lower.y, lower.z }),
		create_vertex({ upper.x, lower.y, lower.z }),
		create_vertex({ upper.x, upper.y, lower.z }),
		create_vertex({ lower.x, upper.y, lower.z }),
		create_vertex({ lower.x, lower.y, upper.z }),
		create_vertex({ upper.x, lower.y, upper.z }),
		create_vertex({ upper.x, upper.y, upper.z }),
		create_vertex({ lower.x, upper.y, upper.z })
	};

	const std::vector<std::uint32_t> indices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		0, 4, 7, 7, 3, 0,
		1, 5, 6, 6, 2, 1,
		0, 1, 5, 5, 4, 0,
		3, 2, 6, 6, 7, 3
	};

	return mesh(vertices, indices);
}
