export module gse.gpu_backend:accel;

import std;

import :core;
import :enums;
import :buffer;
import :bindless;

import gse.core;
import gse.math;

export namespace gse::gpu {
	struct acceleration_structure {
		std::uint64_t value = 0;

		explicit operator bool() const {
			return value != 0;
		}
	};

	enum class acceleration_structure_type : std::uint8_t {
		top_level,
		bottom_level,
	};

	enum class build_acceleration_structure_mode : std::uint8_t {
		build,
		update,
	};

	enum class build_acceleration_structure_flag : std::uint32_t {
		allow_update = 1u << 0,
		allow_compaction = 1u << 1,
		prefer_fast_trace = 1u << 2,
		prefer_fast_build = 1u << 3,
		low_memory = 1u << 4,
	};

	using build_acceleration_structure_flags = gse::flags<build_acceleration_structure_flag>;
	constexpr auto operator|(build_acceleration_structure_flag a, build_acceleration_structure_flag b) -> build_acceleration_structure_flags {
		return build_acceleration_structure_flags(a) | b;
	}

	enum class geometry_flag : std::uint8_t {
		opaque = 1 << 0,
		no_duplicate_any_hit_invocation = 1 << 1,
	};

	using geometry_flags = gse::flags<geometry_flag>;
	constexpr auto operator|(geometry_flag a, geometry_flag b) -> geometry_flags {
		return geometry_flags(a) | b;
	}

	struct acceleration_structure_geometry_triangles_data {
		vertex_format vertex_format = vertex_format::r32g32b32_sfloat;
		device_address vertex_data = 0;
		device_size vertex_stride = 0;
		std::uint32_t max_vertex = 0;
		index_type index_type = index_type::uint32;
		device_address index_data = 0;
	};

	struct acceleration_structure_geometry_instances_data {
		bool array_of_pointers = false;
		device_address data = 0;
	};

	enum class acceleration_structure_geometry_type : std::uint8_t {
		triangles,
		instances,
	};

	struct acceleration_structure_geometry {
		acceleration_structure_geometry_type type = acceleration_structure_geometry_type::instances;
		acceleration_structure_geometry_triangles_data triangles;
		acceleration_structure_geometry_instances_data instances;
		geometry_flags flags;
	};

	struct acceleration_structure_build_geometry_info {
		acceleration_structure_type type = acceleration_structure_type::bottom_level;
		build_acceleration_structure_flags flags;
		build_acceleration_structure_mode mode = build_acceleration_structure_mode::build;
		acceleration_structure dst;
		std::span<const acceleration_structure_geometry> geometries;
		device_address scratch_address = 0;
	};

	struct acceleration_structure_build_range_info {
		std::uint32_t primitive_count = 0;
		std::uint32_t primitive_offset = 0;
		std::uint32_t first_vertex = 0;
		std::uint32_t transform_offset = 0;
	};

	struct acceleration_structure_build_sizes {
		device_size acceleration_structure_size = 0;
		device_size build_scratch_size = 0;
		device_size update_scratch_size = 0;
	};

	enum class acceleration_structure_instance_flag : std::uint8_t {
		triangle_cull_disable = 1 << 0,
		triangle_flip_facing = 1 << 1,
		force_opaque = 1 << 2,
		force_no_opaque = 1 << 3,
	};

	struct acceleration_structure_instance {
		std::array<float, 12> transform{};
		std::uint32_t custom_index : 24 = 0;
		std::uint32_t mask : 8 = 0;
		std::uint32_t sbt_offset : 24 = 0;
		std::uint32_t flags : 8 = 0;
		device_address blas_address = 0;
	};

	static_assert(sizeof(acceleration_structure_instance) == 64);
	static_assert(alignof(acceleration_structure_instance) == 8);

	struct tlas_instance_desc {
		spatial_matrix transform = spatial_matrix(1.0f);
		std::uint32_t custom_index = 0;
		std::uint8_t mask = 0xFF;
		std::uint32_t sbt_offset = 0;
		bool cull_disable = true;
		std::uint64_t blas_address = 0;
	};

	[[nodiscard]]
	auto pack_instance(
		const tlas_instance_desc& inst
	) -> acceleration_structure_instance;

	class blas final : public non_copyable {
	public:
		blas() {}
		blas(
			gpu::buffer storage,
			gpu::acceleration_structure acceleration_structure,
			gpu::device_address device_address,
			gpu::bindless_handle heap_handle = {}
		);

		~blas() = default;

		blas(
			blas&&
		) noexcept = default;

		auto operator=(
			blas&&
		) noexcept -> blas& = default;

		[[nodiscard]] auto handle() const -> gpu::acceleration_structure;

		[[nodiscard]] auto device_address() const -> gpu::device_address;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::buffer m_storage;
		gpu::acceleration_structure m_acceleration_structure;
		gpu::device_address m_device_address = 0;
		gpu::bindless_handle m_heap_handle;
	};

	class tlas final : public non_copyable {
	public:
		tlas() {}
		tlas(
			gpu::buffer storage,
			gpu::buffer scratch,
			gpu::buffer instance_buffer,
			gpu::acceleration_structure acceleration_structure,
			gpu::device_address device_address
		);

		~tlas() = default;

		tlas(
			tlas&&
		) noexcept = default;

		auto operator=(
			tlas&&
		) noexcept -> tlas& = default;

		[[nodiscard]] auto handle() const -> gpu::acceleration_structure;

		[[nodiscard]] auto device_address() const -> gpu::device_address;

		[[nodiscard]] auto instance_buffer(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto scratch_buffer() const -> const gpu::buffer&;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::buffer m_storage;
		gpu::buffer m_scratch;
		gpu::buffer m_instance_buffer;
		gpu::acceleration_structure m_acceleration_structure;
		gpu::device_address m_device_address = 0;
	};
}

gse::gpu::blas::blas(gpu::buffer storage, const gpu::acceleration_structure acceleration_structure, const gpu::device_address device_address, gpu::bindless_handle heap_handle)
	: m_storage(std::move(storage)), m_acceleration_structure(acceleration_structure), m_device_address(device_address), m_heap_handle(std::move(heap_handle)) {
}

auto gse::gpu::blas::handle() const -> gpu::acceleration_structure {
	return m_acceleration_structure;
}

auto gse::gpu::blas::device_address() const -> gpu::device_address {
	return m_device_address;
}

auto gse::gpu::blas::valid() const -> bool {
	return static_cast<bool>(m_acceleration_structure);
}

gse::gpu::tlas::tlas(gpu::buffer storage, gpu::buffer scratch, gpu::buffer instance_buffer, const gpu::acceleration_structure acceleration_structure, const gpu::device_address device_address)
	: m_storage(std::move(storage)), m_scratch(std::move(scratch)), m_instance_buffer(std::move(instance_buffer)), m_acceleration_structure(acceleration_structure), m_device_address(device_address) {
}

auto gse::gpu::tlas::handle() const -> gpu::acceleration_structure {
	return m_acceleration_structure;
}

auto gse::gpu::tlas::device_address() const -> gpu::device_address {
	return m_device_address;
}

auto gse::gpu::tlas::instance_buffer(this auto& self) -> auto& {
	return self.m_instance_buffer;
}

auto gse::gpu::tlas::scratch_buffer() const -> const gpu::buffer& {
	return m_scratch;
}

auto gse::gpu::tlas::valid() const -> bool {
	return static_cast<bool>(m_acceleration_structure);
}

auto gse::gpu::pack_instance(const tlas_instance_desc& inst) -> acceleration_structure_instance {
	const auto transposed = inst.transform.transpose();
	acceleration_structure_instance out{};
	std::memcpy(out.transform.data(), &transposed, sizeof(out.transform));
	out.custom_index = inst.custom_index;
	out.mask = inst.mask;
	out.sbt_offset = inst.sbt_offset;
	out.flags = inst.cull_disable
		? static_cast<std::uint8_t>(acceleration_structure_instance_flag::triangle_cull_disable)
		: 0;
	out.blas_address = inst.blas_address;
	return out;
}
