export module gse.graphics.raytracing;

import std;
import vulkan;

import gse.gpu;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;
import gse.log;

export namespace gse::gpu {
	struct blas_geometry_desc {
		const buffer* vertex_buffer = nullptr;
		std::uint32_t vertex_count  = 0;
		std::uint32_t vertex_stride = 0;
		const buffer* index_buffer  = nullptr;
		std::uint32_t index_count   = 0;
	};

	struct acceleration_structure_instance {
		mat4f transform = mat4(1.0f);
		std::uint32_t custom_index = 0;
		std::uint8_t mask = 0xFF;
		std::uint32_t sbt_offset = 0;
		bool cull_disable = true;
		std::uint64_t blas_address = 0;
	};

	class blas final : public non_copyable {
	public:
		blas() = default;
		blas(
			buffer&& storage,
			vk::raii::AccelerationStructureKHR&& handle,
			vk::DeviceAddress addr
		);
		blas(blas&&) noexcept = default;
		auto operator=(blas&&) noexcept -> blas& = default;

		[[nodiscard]] auto native_handle
		(this const blas& self
		) -> acceleration_structure_handle;

		[[nodiscard]] auto device_address(
			this const blas& self
		) -> std::uint64_t;

		explicit operator bool(
		) const;
	private:
		buffer m_storage;
		vk::raii::AccelerationStructureKHR m_handle{ nullptr };
		vk::DeviceAddress m_device_address = 0;
	};

	class tlas final : public non_copyable {
	public:
		tlas() = default;
		tlas(
			buffer&& storage,
			buffer&& scratch,
			buffer&& instance_buffer,
			vk::raii::AccelerationStructureKHR&& handle
		);

		tlas(tlas&&) noexcept = default;
		auto operator=(tlas&&) noexcept -> tlas& = default;

		[[nodiscard]] auto native_handle(
			this const tlas& self
		) -> acceleration_structure_handle;

		[[nodiscard]] auto instances_buffer(
			this tlas& self
		) -> buffer&;

		explicit operator bool(
		) const;
	private:
		buffer m_storage;
		buffer m_scratch;
		buffer m_instance_buffer;
		vk::raii::AccelerationStructureKHR m_handle{ nullptr };

		friend auto rebuild_tlas(context&, tlas&, std::span<const acceleration_structure_instance>, vulkan::recording_context&) -> void;
		friend auto write_tlas_instances(tlas&, std::span<const acceleration_structure_instance>) -> void;
		friend auto build_tlas_in_place(context&, tlas&, std::uint32_t, vulkan::recording_context&) -> void;
	};

	auto build_blas(
		context& ctx,
		const blas_geometry_desc& desc
	) -> blas;

	auto build_tlas(
		context& ctx,
		std::uint32_t max_instances
	) -> tlas;

	auto rebuild_tlas(
		context& ctx,
		tlas& t,
		std::span<const acceleration_structure_instance> instances,
		vulkan::recording_context& rec
	) -> void;

	auto write_tlas_instances(
		tlas& t,
		std::span<const acceleration_structure_instance> instances
	) -> void;

	auto build_tlas_in_place(
		context& ctx,
		tlas& t,
		std::uint32_t instance_count,
		vulkan::recording_context& rec
	) -> void;

}

namespace gse::gpu {
	auto to_vk_instance(
		const acceleration_structure_instance& inst
	) -> vk::AccelerationStructureInstanceKHR;

	auto acceleration_structure_scratch_alignment(
		device& dev
	) -> vk::DeviceSize;
}

auto gse::gpu::to_vk_instance(const acceleration_structure_instance& inst) -> vk::AccelerationStructureInstanceKHR {
	vk::TransformMatrixKHR transform{};
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 4; ++col) {
			transform.matrix[row][col] = inst.transform[col][row];
		}
	}

	return {
		.transform = transform,
		.instanceCustomIndex = inst.custom_index,
		.mask = inst.mask,
		.instanceShaderBindingTableRecordOffset = inst.sbt_offset,
		.flags = static_cast<std::uint8_t>(inst.cull_disable ? vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable : vk::GeometryInstanceFlagBitsKHR{}),
		.accelerationStructureReference = inst.blas_address
	};
}

auto gse::gpu::acceleration_structure_scratch_alignment(device& dev) -> vk::DeviceSize {
	return vulkan::scratch_offset_alignment(dev.vulkan_device());
}

namespace {
	auto build_blas_async(
		gse::gpu::device& dev,
		gse::gpu::acceleration_structure_handle as_handle,
		gse::gpu::acceleration_structure_geometry geometry,
		std::uint32_t prim_count,
		gse::gpu::device_size scratch_size,
		gse::gpu::device_size scratch_alignment
	) -> gse::async::task<> {
		const auto& dev_cfg = dev.vulkan_device();

		auto scratch = dev_cfg.create_buffer(gse::gpu::buffer_create_info{
			.size = scratch_size + scratch_alignment,
			.usage = gse::gpu::buffer_flag::storage,
		}, nullptr);

		const auto scratch_addr = gse::vulkan::buffer_device_address(dev_cfg, scratch.handle());
		const gse::gpu::device_address aligned_scratch = (scratch_addr + scratch_alignment - 1) & ~(scratch_alignment - 1);

		auto cmd = co_await begin_transient(dev, gse::gpu::queue_id::graphics);

		const gse::gpu::acceleration_structure_build_geometry_info build_info{
			.type = gse::gpu::acceleration_structure_type::bottom_level,
			.flags = gse::gpu::build_acceleration_structure_flag::prefer_fast_build,
			.mode = gse::gpu::build_acceleration_structure_mode::build,
			.dst = as_handle,
			.geometries = std::span(&geometry, 1),
			.scratch_address = aligned_scratch,
		};

		const gse::gpu::acceleration_structure_build_range_info range{
			.primitive_count = prim_count,
		};
		const gse::gpu::acceleration_structure_build_range_info* range_ptr = &range;

		const auto cmd_handle = cmd.handle();
		gse::vulkan::commands(cmd_handle).build_acceleration_structures(build_info, std::span(&range_ptr, 1));

		const gse::gpu::memory_barrier barrier{
			.src_stages = gse::gpu::pipeline_stage_flag::acceleration_structure_build,
			.src_access = gse::gpu::access_flag::acceleration_structure_write,
			.dst_stages = gse::gpu::pipeline_stage_flag::acceleration_structure_build,
			.dst_access = gse::gpu::access_flag::acceleration_structure_read,
		};
		gse::vulkan::commands(cmd_handle).pipeline_barrier(gse::gpu::dependency_info{ .memory_barriers = std::span(&barrier, 1) });

		co_await submit(dev, std::move(cmd), gse::gpu::queue_id::graphics)
			.retain(std::move(scratch));
	}
}

auto gse::gpu::build_blas(context& ctx, const blas_geometry_desc& desc) -> blas {
	auto& dev = ctx.device();
	const auto& dev_cfg = dev.vulkan_device();

	const auto vertex_addr = vulkan::buffer_device_address(dev_cfg, desc.vertex_buffer->handle());
	const auto index_addr = vulkan::buffer_device_address(dev_cfg, desc.index_buffer->handle());

	const gpu::acceleration_structure_geometry geometry{
		.type = gpu::acceleration_structure_geometry_type::triangles,
		.triangles = {
			.vertex_format = gpu::vertex_format::r32g32b32_sfloat,
			.vertex_data = vertex_addr,
			.vertex_stride = desc.vertex_stride,
			.max_vertex = desc.vertex_count - 1,
			.index_type = gpu::index_type::uint32,
			.index_data = index_addr,
		},
		.flags = gpu::geometry_flag::opaque,
	};

	const std::uint32_t prim_count = desc.index_count / 3;
	const auto sizes = vulkan::query_blas_build_sizes(dev_cfg, geometry, prim_count);

	auto storage_buf = create_buffer(ctx, {
		.size = sizes.acceleration_structure_size,
		.usage = buffer_flag::acceleration_structure_storage,
	});

	auto as = vulkan::create_acceleration_structure(
		dev_cfg,
		storage_buf.handle(),
		sizes.acceleration_structure_size,
		gpu::acceleration_structure_type::bottom_level
	);

	const auto as_handle = gpu::acceleration_structure_handle{ .value = std::bit_cast<std::uint64_t>(*as) };

	const auto scratch_alignment = vulkan::scratch_offset_alignment(dev_cfg);

	dispatch(dev, build_blas_async(dev, as_handle, geometry, prim_count, sizes.build_scratch_size, scratch_alignment));

	const auto device_address = vulkan::acceleration_structure_address(dev_cfg, as);

	return blas{
		std::move(storage_buf),
		std::move(as),
		device_address,
	};
}

auto gse::gpu::build_tlas(context& ctx, const std::uint32_t max_instances) -> tlas {
	auto& dev = ctx.device();
	const auto& dev_cfg = dev.vulkan_device();

	const auto sizes = vulkan::query_tlas_build_sizes(dev_cfg, max_instances);

	auto storage_buf = create_buffer(ctx, {
		.size = sizes.acceleration_structure_size,
		.usage = buffer_flag::acceleration_structure_storage,
	});

	const auto scratch_alignment = vulkan::scratch_offset_alignment(dev_cfg);
	auto scratch_buf = buffer(ctx.allocator().create_buffer(gpu::buffer_create_info{
		.size = std::max(sizes.build_scratch_size, sizes.update_scratch_size) + scratch_alignment,
		.usage = gpu::buffer_flag::storage,
	}));

	const gpu::device_size instance_buf_size = max_instances * sizeof(vk::AccelerationStructureInstanceKHR);
	auto instance_buf = ctx.allocator().create_buffer(gpu::buffer_create_info{
		.size = instance_buf_size,
		.usage = gpu::buffer_flag::acceleration_structure_build_input | gpu::buffer_flag::storage,
	});

	auto as = vulkan::create_acceleration_structure(
		dev_cfg,
		storage_buf.handle(),
		sizes.acceleration_structure_size,
		gpu::acceleration_structure_type::top_level
	);

	return tlas{
		std::move(storage_buf),
		std::move(scratch_buf),
		buffer(std::move(instance_buf)),
		std::move(as),
	};
}

auto gse::gpu::rebuild_tlas(context& ctx, tlas& t, const std::span<const acceleration_structure_instance> instances, vulkan::recording_context& rec) -> void {
	auto& dev = ctx.device();
	const auto& dev_cfg = dev.vulkan_device();

	std::vector<vk::AccelerationStructureInstanceKHR> vk_instances;
	vk_instances.reserve(instances.size());
	for (const auto& inst : instances) {
		vk_instances.push_back(to_vk_instance(inst));
	}

	if (auto* mapped = t.m_instance_buffer.mapped(); mapped && !vk_instances.empty()) {
		std::memcpy(mapped, vk_instances.data(), vk_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
	}

	const auto instance_addr = vulkan::buffer_device_address(dev_cfg, t.m_instance_buffer.handle());
	const auto scratch_alignment = vulkan::scratch_offset_alignment(dev_cfg);
	const auto scratch_raw = vulkan::buffer_device_address(dev_cfg, t.m_scratch.handle());
	const gpu::device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	const gpu::memory_barrier host_write_barrier{
		.src_stages = gpu::pipeline_stage_flag::host,
		.src_access = gpu::access_flag::host_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.dst_access = gpu::access_flag::acceleration_structure_read,
	};
	rec.pipeline_barrier(gpu::dependency_info{ .memory_barriers = std::span(&host_write_barrier, 1) });

	const gpu::acceleration_structure_geometry geometry{
		.type = gpu::acceleration_structure_geometry_type::instances,
		.instances = {
			.array_of_pointers = false,
			.data = instance_addr,
		},
	};

	const gpu::acceleration_structure_build_geometry_info build_info{
		.type = gpu::acceleration_structure_type::top_level,
		.flags = gpu::build_acceleration_structure_flag::prefer_fast_build | gpu::build_acceleration_structure_flag::allow_update,
		.mode = gpu::build_acceleration_structure_mode::build,
		.dst = gpu::acceleration_structure_handle{ .value = std::bit_cast<std::uint64_t>(*t.m_handle) },
		.geometries = std::span(&geometry, 1),
		.scratch_address = scratch_addr,
	};

	const gpu::acceleration_structure_build_range_info range{
		.primitive_count = static_cast<std::uint32_t>(vk_instances.size()),
	};
	const gpu::acceleration_structure_build_range_info* range_ptr = &range;

	rec.build_acceleration_structure(build_info, { &range_ptr, 1 });

	const gpu::memory_barrier barrier{
		.src_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.src_access = gpu::access_flag::acceleration_structure_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build | gpu::pipeline_stage_flag::fragment_shader,
		.dst_access = gpu::access_flag::acceleration_structure_read | gpu::access_flag::acceleration_structure_write,
	};
	rec.pipeline_barrier(gpu::dependency_info{ .memory_barriers = std::span(&barrier, 1) });
}

gse::gpu::blas::blas(buffer&& storage, vk::raii::AccelerationStructureKHR&& handle, const vk::DeviceAddress addr) : m_storage(std::move(storage)), m_handle(std::move(handle)), m_device_address(addr) {}

auto gse::gpu::blas::native_handle(this const blas& self) -> acceleration_structure_handle {
	return { std::bit_cast<std::uint64_t>(*self.m_handle) };
}

auto gse::gpu::blas::device_address(this const blas& self) -> std::uint64_t {
	return self.m_device_address;
}

gse::gpu::blas::operator bool() const {
	return *m_handle != nullptr;
}

gse::gpu::tlas::tlas(buffer&& storage, buffer&& scratch, buffer&& instance_buffer, vk::raii::AccelerationStructureKHR&& handle) : m_storage(std::move(storage)), m_scratch(std::move(scratch)), m_instance_buffer(std::move(instance_buffer)), m_handle(std::move(handle)) {}

auto gse::gpu::tlas::native_handle(this const tlas& self) -> acceleration_structure_handle {
	return { std::bit_cast<std::uint64_t>(*self.m_handle) };
}

auto gse::gpu::tlas::instances_buffer(this tlas& self) -> buffer& {
	return self.m_instance_buffer;
}

gse::gpu::tlas::operator bool() const {
	return *m_handle != nullptr;
}

auto gse::gpu::write_tlas_instances(tlas& t, const std::span<const acceleration_structure_instance> instances) -> void {
	std::vector<vk::AccelerationStructureInstanceKHR> vk_instances;
	vk_instances.reserve(instances.size());
	for (const auto& inst : instances) {
		vk_instances.push_back(to_vk_instance(inst));
	}

	if (auto* mapped = t.m_instance_buffer.mapped(); mapped && !vk_instances.empty()) {
		std::memcpy(mapped, vk_instances.data(), vk_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
	}
}

auto gse::gpu::build_tlas_in_place(context& ctx, tlas& t, const std::uint32_t instance_count, vulkan::recording_context& rec) -> void {
	const auto& dev_cfg = ctx.device().device();

	const auto instance_addr = vulkan::buffer_device_address(dev_cfg, t.m_instance_buffer.handle());
	const auto scratch_alignment = vulkan::scratch_offset_alignment(dev_cfg);
	const auto scratch_raw = vulkan::buffer_device_address(dev_cfg, t.m_scratch.handle());
	const gpu::device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	const gpu::memory_barrier pre_barrier{
		.src_stages = gpu::pipeline_stage_flag::compute_shader,
		.src_access = gpu::access_flag::shader_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.dst_access = gpu::access_flag::acceleration_structure_read,
	};
	rec.pipeline_barrier(gpu::dependency_info{ .memory_barriers = std::span(&pre_barrier, 1) });

	const gpu::acceleration_structure_geometry geometry{
		.type = gpu::acceleration_structure_geometry_type::instances,
		.instances = {
			.array_of_pointers = false,
			.data = instance_addr,
		},
	};

	const gpu::acceleration_structure_build_geometry_info build_info{
		.type = gpu::acceleration_structure_type::top_level,
		.flags = gpu::build_acceleration_structure_flag::prefer_fast_build | gpu::build_acceleration_structure_flag::allow_update,
		.mode = gpu::build_acceleration_structure_mode::build,
		.dst = gpu::acceleration_structure_handle{ .value = std::bit_cast<std::uint64_t>(*t.m_handle) },
		.geometries = std::span(&geometry, 1),
		.scratch_address = scratch_addr,
	};

	const gpu::acceleration_structure_build_range_info range{
		.primitive_count = instance_count,
	};
	const gpu::acceleration_structure_build_range_info* range_ptr = &range;

	rec.build_acceleration_structure(build_info, { &range_ptr, 1 });

	const gpu::memory_barrier post_barrier{
		.src_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.src_access = gpu::access_flag::acceleration_structure_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build | gpu::pipeline_stage_flag::fragment_shader,
		.dst_access = gpu::access_flag::acceleration_structure_read | gpu::access_flag::acceleration_structure_write,
	};
	rec.pipeline_barrier(gpu::dependency_info{ .memory_barriers = std::span(&post_barrier, 1) });
}
