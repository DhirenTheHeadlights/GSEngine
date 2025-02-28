export module gse.graphics.mesh;

import std;
import vulkan_hpp;

import gse.graphics.material;
import gse.platform.context;
import gse.platform.assert;
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
		mesh(const mesh_data& data);
		mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, material_handle material = {});
        ~mesh();

        mesh(const mesh&) = delete;
        auto operator=(const mesh&) -> mesh & = delete;
        mesh(mesh&& other) noexcept;
        auto operator=(mesh&& other) noexcept -> mesh&;

        auto initialize() -> void;
        auto destroy() const -> void;

        auto bind(vk::CommandBuffer command_buffer) const -> void;
        auto draw(vk::CommandBuffer command_buffer) const -> void;

        vk::Buffer vertex_buffer;
        vk::DeviceMemory vertex_memory;
        vk::Buffer index_buffer;
        vk::DeviceMemory index_memory;

        std::vector<vertex> vertices;
        std::vector<std::uint32_t> indices;
		material_handle material;

		vec3<length> center_of_mass;
    };

    auto calculate_center_of_mass(const std::vector<std::uint32_t>& indices, const std::vector<vertex>& vertices) -> vec3<length>;
	auto generate_bounding_box_mesh(const axis_aligned_bounding_box& aabb) -> mesh;
}

gse::mesh::mesh(const mesh_data& data) : vertices(std::move(data.vertices)), indices(std::move(data.indices)), material(data.material) {
    initialize();
	center_of_mass = calculate_center_of_mass(indices, vertices);
}

gse::mesh::mesh(const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const material_handle material)
	: vertices(vertices), indices(indices), material(material) {
	initialize();
	center_of_mass = calculate_center_of_mass(indices, vertices);
}

gse::mesh::~mesh() {
    destroy();
}

gse::mesh::mesh(mesh&& other) noexcept
	: vertex_buffer(other.vertex_buffer), vertex_memory(other.vertex_memory),
	  index_buffer(other.index_buffer), index_memory(other.index_memory),
	  vertices(std::move(other.vertices)), indices(std::move(other.indices)), material(nullptr),
	  center_of_mass(other.center_of_mass) {
	other.vertex_buffer = nullptr;
	other.vertex_memory = nullptr;
	other.index_buffer = nullptr;
	other.index_memory = nullptr;
}

auto gse::mesh::operator=(mesh&& other) noexcept -> mesh& {
    if (this != &other) {
        destroy();

        vertex_buffer = other.vertex_buffer;
        vertex_memory = other.vertex_memory;
        index_buffer = other.index_buffer;
        index_memory = other.index_memory;
        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
		center_of_mass = other.center_of_mass;

        other.vertex_buffer = nullptr;
        other.vertex_memory = nullptr;
        other.index_buffer = nullptr;
        other.index_memory = nullptr;
    }
    return *this;
}

auto gse::mesh::initialize() -> void {
	const auto device = vulkan::get_device_config().device;

    const vk::DeviceSize vertex_size = sizeof(vertex) * vertices.size();
    const vk::DeviceSize index_size = sizeof(std::uint32_t) * indices.size();

    const vk::BufferCreateInfo vertex_buffer_info(
        {}, vertex_size,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive
    );
    vertex_buffer = device.createBuffer(vertex_buffer_info);

    const vk::MemoryRequirements vertex_mem_req = device.getBufferMemoryRequirements(vertex_buffer);
    const vk::MemoryAllocateInfo vertex_alloc_info(
        vertex_mem_req.size,
        vulkan::find_memory_type(vertex_mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
    );
    vertex_memory = device.allocateMemory(vertex_alloc_info);
    device.bindBufferMemory(vertex_buffer, vertex_memory, 0);

    const vk::BufferCreateInfo index_buffer_info(
        {}, index_size,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive
    );
    index_buffer = device.createBuffer(index_buffer_info);

    const vk::MemoryRequirements index_mem_req = device.getBufferMemoryRequirements(index_buffer);
    const vk::MemoryAllocateInfo index_alloc_info(
        index_mem_req.size,
        vulkan::find_memory_type(index_mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
    );

    index_memory = device.allocateMemory(index_alloc_info);
    device.bindBufferMemory(index_buffer, index_memory, 0);
}

auto gse::mesh::destroy() const -> void {
	const auto device = vulkan::get_device_config().device;
    if (vertex_buffer) {
        device.destroyBuffer(vertex_buffer);
        device.freeMemory(vertex_memory);
    }
    if (index_buffer) {
        device.destroyBuffer(index_buffer);
        device.freeMemory(index_memory);
    }
}

auto gse::mesh::bind(const vk::CommandBuffer command_buffer) const -> void {
    command_buffer.bindVertexBuffers(0, vertex_buffer, {});
    command_buffer.bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
}

auto gse::mesh::draw(const vk::CommandBuffer command_buffer) const -> void {
    command_buffer.drawIndexed(static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
}

auto gse::calculate_center_of_mass(const std::vector<std::uint32_t>& indices, const std::vector<vertex>& vertices) -> vec3<length> {
    constexpr unitless::vec3d reference_point(0.f);

    double total_volume = 0.f;
    unitless::vec3 moment(0.f);

    perma_assert(indices.size() % 3 == 0, "Indices count is not a multiple of 3. Ensure that each face is defined by exactly three indices.");

    for (size_t i = 0; i < indices.size(); i += 3) {
        const unsigned int idx0 = indices[i];
        const unsigned int idx1 = indices[i + 1];
        const unsigned int idx2 = indices[i + 2];

        perma_assert(idx0 <= vertices.size() || idx1 <= vertices.size() || idx2 <= vertices.size(), "Index out of range while accessing vertices.");

        const unitless::vec3d v0(vertices[idx0].position);
        const unitless::vec3d v1(vertices[idx1].position);
        const unitless::vec3d v2(vertices[idx2].position);

        unitless::vec3d a = v0 - reference_point;
        unitless::vec3d b = v1 - reference_point;
        unitless::vec3d c = v2 - reference_point;

        auto volume = std::abs(dot(a, cross(b, c)) / 6.0);
        unitless::vec3d tetra_com = (v0 + v1 + v2 + reference_point) / 4.0;

        total_volume += volume;
        moment += tetra_com * volume;
    }

    perma_assert(total_volume != 0.0, "Total volume is zero. Check if the mesh is closed and correctly oriented.");

    return moment / total_volume;
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
