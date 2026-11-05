export module gse.webgpu:commands;

import std;

import gse.assert;
import gse.gpu_backend;
import gse.math;
import gse.webgpu_api;

import :device;

export namespace gse::webgpu {
	class commands {
	public:
		commands() = default;

		explicit commands(
			gpu::command_buffer_handle cmd
		);

		[[nodiscard]] auto native() const -> gpu::command_buffer_handle;

		[[nodiscard]] auto valid() const -> bool;

		auto begin() const -> void;

		auto reset() const -> void;

		auto end() const -> void;

		auto begin_rendering(
			const gpu::rendering_info& info
		) const -> void;

		auto end_rendering() const -> void;

		auto reset_query_pool(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count
		) const -> void;

		auto write_timestamp(
			gpu::pipeline_stage_flags stage,
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto begin_query(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto end_query(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t query_index
		) const -> void;

		auto resolve_query_pool(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count
		) const -> void;

		auto release_swapchain_image_to_present(
			gpu::handle<gpu::image> img,
			gpu::pipeline_stage_flags src_stages,
			gpu::access_flags src_access
		) const -> void;

		auto set_viewport(
			const gpu::viewport& viewport
		) const -> void;

		auto set_scissor(
			const rect_t<vec2i>& scissor
		) const -> void;

		auto set_vertex_input_none() const -> void;

		auto set_topology(
			gpu::topology t
		) const -> void;

		auto set_polygon_mode(
			gpu::polygon_mode m
		) const -> void;

		auto set_cull_mode(
			gpu::cull_mode m
		) const -> void;

		auto set_front_face(
			gpu::front_face f
		) const -> void;

		auto set_depth_test_enable(
			bool enable
		) const -> void;

		auto set_depth_write_enable(
			bool enable
		) const -> void;

		auto set_depth_compare_op(
			gpu::compare_op op
		) const -> void;

		auto set_depth_bias_enable(
			bool enable
		) const -> void;

		auto set_depth_bias(
			float constant,
			float clamp,
			float slope
		) const -> void;

		auto set_depth_clamp_enable(
			bool enable
		) const -> void;

		auto set_depth_bounds_test_enable(
			bool enable
		) const -> void;

		auto set_stencil_test_enable(
			bool enable
		) const -> void;

		auto set_line_width(
			float width
		) const -> void;

		auto set_rasterizer_discard_enable(
			bool enable
		) const -> void;

		auto set_primitive_restart_enable(
			bool enable
		) const -> void;

		auto set_rasterization_samples(
			gpu::sample_count samples
		) const -> void;

		auto set_sample_mask(
			gpu::sample_count samples,
			std::uint32_t mask
		) const -> void;

		auto set_alpha_to_coverage_enable(
			bool enable
		) const -> void;

		auto set_alpha_to_one_enable(
			bool enable
		) const -> void;

		auto set_logic_op_enable(
			bool enable
		) const -> void;

		auto set_color_blend_enable(
			std::uint32_t first_attachment,
			std::span<const std::uint8_t> enables
		) const -> void;

		auto set_color_blend_equation(
			std::uint32_t first_attachment,
			std::span<const gpu::color_blend_equation> equations
		) const -> void;

		auto set_color_write_mask(
			std::uint32_t first_attachment,
			std::span<const gpu::color_component_flags> masks
		) const -> void;

		auto set_blend_constants(
			std::array<float, 4> constants
		) const -> void;

		auto bind_shaders(
			std::span<const gpu::stage_flag> stages,
			std::span<const gpu::handle<gpu::shader_object>> shaders
		) const -> void;

		auto unbind_shaders(
			std::span<const gpu::stage_flag> stages
		) const -> void;

		auto bind_index_buffer_2(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			gpu::device_size size,
			gpu::index_type type
		) const -> void;

		auto bind_resource_heap(
			gpu::device_address heap_address,
			gpu::device_size heap_size,
			gpu::device_size reserved_offset,
			gpu::device_size reserved_size
		) const -> void;

		auto bind_sampler_heap(
			gpu::device_address heap_address,
			gpu::device_size heap_size,
			gpu::device_size reserved_offset,
			gpu::device_size reserved_size
		) const -> void;

		auto push_data(
			std::uint32_t offset,
			std::span<const std::byte> data
		) const -> void;

		auto draw(
			std::uint32_t vertex_count,
			std::uint32_t instance_count,
			std::uint32_t first_vertex,
			std::uint32_t first_instance
		) const -> void;

		auto draw_indexed(
			std::uint32_t index_count,
			std::uint32_t instance_count,
			std::uint32_t first_index,
			std::int32_t vertex_offset,
			std::uint32_t first_instance
		) const -> void;

		auto draw_mesh_tasks(
			std::uint32_t x,
			std::uint32_t y,
			std::uint32_t z
		) const -> void;

		auto draw_indexed_indirect(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto draw_mesh_tasks_indirect(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y,
			std::uint32_t z
		) const -> void;

		auto dispatch_indirect(
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset
		) const -> void;

		auto copy_buffer(
			gpu::handle<gpu::buffer> src,
			gpu::handle<gpu::buffer> dst,
			const gpu::buffer_copy_region& region
		) const -> void;

		auto fill_buffer(
			gpu::handle<gpu::buffer> dst,
			gpu::device_size offset,
			gpu::device_size size,
			std::uint32_t data
		) const -> void;

		auto copy_buffer_to_image(
			gpu::handle<gpu::buffer> src,
			gpu::handle<gpu::image> dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto copy_image_to_buffer(
			gpu::handle<gpu::image> src,
			gpu::handle<gpu::buffer> dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto blit_image(
			gpu::handle<gpu::image> src,
			gpu::handle<gpu::image> dst,
			const gpu::image_blit_region& region,
			gpu::sampler_filter filter
		) const -> void;

		auto copy_image(
			gpu::handle<gpu::image> src,
			gpu::handle<gpu::image> dst,
			const gpu::image_copy_region& region
		) const -> void;

		auto pipeline_barrier(
			const gpu::dependency_info& dep
		) const -> void;

		auto build_acceleration_structures(
			const gpu::acceleration_structure_build_geometry_info& build_info,
			std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
		) const -> void;

	private:
		[[nodiscard]] auto list() const -> command_list*;

		gpu::command_buffer_handle m_cmd{};
	};
}

namespace gse::webgpu {
	auto not_implemented(
		std::string_view operation
	) -> void;

	[[nodiscard]] auto load_op_of(
		gpu::load_op op
	) -> WGPULoadOp;

	[[nodiscard]] auto store_op_of(
		gpu::store_op op
	) -> WGPUStoreOp;

	[[nodiscard]] auto begin_compute_pass(
		command_list& list
	) -> WGPUComputePassEncoder;

	auto end_compute_pass(
		WGPUComputePassEncoder pass
	) -> void;
}

auto gse::webgpu::begin_compute_pass(command_list& list) -> WGPUComputePassEncoder {
	assert(list.encoder != nullptr && list.pass == nullptr, "webgpu: dispatch needs a begun command buffer with no open render pass");
	return wgpuCommandEncoderBeginComputePass(list.encoder, nullptr);
}

auto gse::webgpu::end_compute_pass(const WGPUComputePassEncoder pass) -> void {
	wgpuComputePassEncoderEnd(pass);
	wgpuComputePassEncoderRelease(pass);
}

auto gse::webgpu::not_implemented(const std::string_view operation) -> void {
	assert(false, "webgpu: {} is not implemented yet; this backend only clears and presents until the WGSL and bind-group slices land", operation);
}

auto gse::webgpu::load_op_of(const gpu::load_op op) -> WGPULoadOp {
	switch (op) {
		case gpu::load_op::load: return WGPULoadOp_Load;
		case gpu::load_op::clear: return WGPULoadOp_Clear;
		case gpu::load_op::dont_care: return WGPULoadOp_Clear;
	}
	return WGPULoadOp_Clear;
}

auto gse::webgpu::store_op_of(const gpu::store_op op) -> WGPUStoreOp {
	return op == gpu::store_op::store ? WGPUStoreOp_Store : WGPUStoreOp_Discard;
}

gse::webgpu::commands::commands(const gpu::command_buffer_handle cmd) : m_cmd(cmd) {}

auto gse::webgpu::commands::list() const -> command_list* {
	return record_of<command_list>(m_cmd.value);
}

auto gse::webgpu::commands::native() const -> gpu::command_buffer_handle {
	return m_cmd;
}

auto gse::webgpu::commands::valid() const -> bool {
	return static_cast<bool>(m_cmd);
}

auto gse::webgpu::commands::begin() const -> void {
	list()->owner->cmd_begin(m_cmd);
}

auto gse::webgpu::commands::reset() const -> void {
	list()->owner->cmd_reset(m_cmd);
}

auto gse::webgpu::commands::end() const -> void {
	list()->owner->cmd_end(m_cmd);
}

auto gse::webgpu::commands::begin_rendering(const gpu::rendering_info& info) const -> void {
	auto* cmd_list = list();
	assert(cmd_list->encoder != nullptr && cmd_list->pass == nullptr, "webgpu: begin_rendering needs a begun command buffer with no open pass");

	cmd_list->formats = {};
	std::vector<WGPURenderPassColorAttachment> colors;
	colors.reserve(info.color_attachments.size());
	for (const auto& attachment : info.color_attachments) {
		const auto* view = record_of<view_record>(attachment.image_view);
		if (cmd_list->formats.color_count < max_color_attachments) {
			cmd_list->formats.colors[cmd_list->formats.color_count++] = texture_format_of(view->texture->format);
		}
		cmd_list->attachment_extent = { view->texture->extent.x(), view->texture->extent.y() };
		const auto& c = attachment.color_clear_value;
		colors.push_back({
			.nextInChain = nullptr,
			.view = view->view,
			.depthSlice = depth_slice_undefined,
			.resolveTarget = nullptr,
			.loadOp = load_op_of(attachment.load),
			.storeOp = store_op_of(attachment.store),
			.clearValue = { .r = c.r, .g = c.g, .b = c.b, .a = c.a },
		});
	}

	WGPURenderPassDepthStencilAttachment depth{};
	if (info.depth_attachment) {
		const auto* depth_view = record_of<view_record>(info.depth_attachment->image_view);
		cmd_list->formats.depth = texture_format_of(depth_view->texture->format);
		cmd_list->attachment_extent = { depth_view->texture->extent.x(), depth_view->texture->extent.y() };
		depth.view = depth_view->view;
		depth.depthLoadOp = load_op_of(info.depth_attachment->load);
		depth.depthStoreOp = store_op_of(info.depth_attachment->store);
		depth.depthClearValue = info.depth_attachment->depth_clear_value.depth;
	}

	const WGPURenderPassDescriptor desc{
		.nextInChain = nullptr,
		.label = { .data = nullptr, .length = 0 },
		.colorAttachmentCount = colors.size(),
		.colorAttachments = colors.data(),
		.depthStencilAttachment = info.depth_attachment ? &depth : nullptr,
		.occlusionQuerySet = nullptr,
		.timestampWrites = nullptr,
	};
	cmd_list->pass = wgpuCommandEncoderBeginRenderPass(cmd_list->encoder, &desc);
}

auto gse::webgpu::commands::end_rendering() const -> void {
	auto* cmd_list = list();
	assert(cmd_list->pass != nullptr, "webgpu: end_rendering with no open pass");
	wgpuRenderPassEncoderEnd(cmd_list->pass);
	wgpuRenderPassEncoderRelease(cmd_list->pass);
	cmd_list->pass = nullptr;
}

auto gse::webgpu::commands::reset_query_pool(const gpu::handle<gpu::query_pool> pool, std::uint32_t, std::uint32_t) const -> void {
	assert(!pool, "webgpu: query pools are never created on this backend");
}

auto gse::webgpu::commands::write_timestamp(gpu::pipeline_stage_flags, const gpu::handle<gpu::query_pool> pool, std::uint32_t) const -> void {
	assert(!pool, "webgpu: query pools are never created on this backend");
}

auto gse::webgpu::commands::begin_query(const gpu::handle<gpu::query_pool> pool, std::uint32_t) const -> void {
	assert(!pool, "webgpu: query pools are never created on this backend");
}

auto gse::webgpu::commands::end_query(const gpu::handle<gpu::query_pool> pool, std::uint32_t) const -> void {
	assert(!pool, "webgpu: query pools are never created on this backend");
}

auto gse::webgpu::commands::resolve_query_pool(const gpu::handle<gpu::query_pool> pool, std::uint32_t, std::uint32_t) const -> void {
	assert(!pool, "webgpu: query pools are never created on this backend");
}

auto gse::webgpu::commands::release_swapchain_image_to_present(gpu::handle<gpu::image>, gpu::pipeline_stage_flags, gpu::access_flags) const -> void {
}

auto gse::webgpu::commands::set_viewport(const gpu::viewport& viewport) const -> void {
	list()->viewport = viewport;
}

auto gse::webgpu::commands::set_scissor(const rect_t<vec2i>& scissor) const -> void {
	list()->scissor = scissor;
}

auto gse::webgpu::commands::set_vertex_input_none() const -> void {
}

auto gse::webgpu::commands::set_topology(gpu::topology) const -> void {
}

auto gse::webgpu::commands::set_polygon_mode(gpu::polygon_mode) const -> void {
}

auto gse::webgpu::commands::set_cull_mode(gpu::cull_mode) const -> void {
}

auto gse::webgpu::commands::set_front_face(gpu::front_face) const -> void {
}

auto gse::webgpu::commands::set_depth_test_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_depth_write_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_depth_compare_op(gpu::compare_op) const -> void {
}

auto gse::webgpu::commands::set_depth_bias_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_depth_bias(float, float, float) const -> void {
}

auto gse::webgpu::commands::set_depth_clamp_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_depth_bounds_test_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_stencil_test_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_line_width(float) const -> void {
}

auto gse::webgpu::commands::set_rasterizer_discard_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_primitive_restart_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_rasterization_samples(gpu::sample_count) const -> void {
}

auto gse::webgpu::commands::set_sample_mask(gpu::sample_count, std::uint32_t) const -> void {
}

auto gse::webgpu::commands::set_alpha_to_coverage_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_alpha_to_one_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_logic_op_enable(bool) const -> void {
}

auto gse::webgpu::commands::set_color_blend_enable(std::uint32_t, std::span<const std::uint8_t>) const -> void {
}

auto gse::webgpu::commands::set_color_blend_equation(std::uint32_t, std::span<const gpu::color_blend_equation>) const -> void {
}

auto gse::webgpu::commands::set_color_write_mask(std::uint32_t, std::span<const gpu::color_component_flags>) const -> void {
}

auto gse::webgpu::commands::set_blend_constants(std::array<float, 4>) const -> void {
}

auto gse::webgpu::commands::bind_shaders(std::span<const gpu::stage_flag>, const std::span<const gpu::handle<gpu::shader_object>> shaders) const -> void {
	for (const auto shader : shaders) {
		if (shader) {
			list()->program = record_of<program_record>(shader);
			return;
		}
	}
}

auto gse::webgpu::commands::unbind_shaders(std::span<const gpu::stage_flag>) const -> void {
}

auto gse::webgpu::commands::bind_index_buffer_2(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, gpu::device_size, const gpu::index_type type) const -> void {
	auto* cmd_list = list();
	cmd_list->index_buffer = record_of<buffer_record>(buffer)->buffer;
	cmd_list->index_offset = offset;
	cmd_list->index_format = type == gpu::index_type::uint16 ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32;
}

auto gse::webgpu::commands::bind_resource_heap(gpu::device_address, gpu::device_size, gpu::device_size, gpu::device_size) const -> void {
}

auto gse::webgpu::commands::bind_sampler_heap(gpu::device_address, gpu::device_size, gpu::device_size, gpu::device_size) const -> void {
}

auto gse::webgpu::commands::push_data(const std::uint32_t offset, const std::span<const std::byte> data) const -> void {
	auto& push = list()->push;
	if (push.size() < offset + data.size()) {
		push.resize(offset + data.size());
	}
	std::memcpy(push.data() + offset, data.data(), data.size());
}

auto gse::webgpu::commands::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	auto* cmd_list = list();
	cmd_list->owner->bind_for_draw(*cmd_list);
	wgpuRenderPassEncoderDraw(cmd_list->pass, vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::webgpu::commands::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	auto* cmd_list = list();
	cmd_list->owner->bind_for_draw(*cmd_list);
	wgpuRenderPassEncoderDrawIndexed(cmd_list->pass, index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::webgpu::commands::draw_mesh_tasks(std::uint32_t, std::uint32_t, std::uint32_t) const -> void {
	not_implemented("draw_mesh_tasks");
}

auto gse::webgpu::commands::draw_indexed_indirect(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	auto* cmd_list = list();
	cmd_list->owner->bind_for_draw(*cmd_list);
	auto* indirect = record_of<buffer_record>(buffer)->buffer;
	for (std::uint32_t i = 0; i < draw_count; ++i) {
		wgpuRenderPassEncoderDrawIndexedIndirect(cmd_list->pass, indirect, offset + static_cast<gpu::device_size>(i) * stride);
	}
}

auto gse::webgpu::commands::draw_mesh_tasks_indirect(gpu::handle<gpu::buffer>, gpu::device_size, std::uint32_t, std::uint32_t) const -> void {
	not_implemented("draw_mesh_tasks_indirect");
}

auto gse::webgpu::commands::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	auto* cmd_list = list();
	auto* pass = begin_compute_pass(*cmd_list);
	cmd_list->owner->bind_for_dispatch(*cmd_list, pass);
	wgpuComputePassEncoderDispatchWorkgroups(pass, x, y, z);
	end_compute_pass(pass);
}

auto gse::webgpu::commands::dispatch_indirect(const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset) const -> void {
	auto* cmd_list = list();
	auto* pass = begin_compute_pass(*cmd_list);
	cmd_list->owner->bind_for_dispatch(*cmd_list, pass);
	wgpuComputePassEncoderDispatchWorkgroupsIndirect(pass, record_of<buffer_record>(buffer)->buffer, offset);
	end_compute_pass(pass);
}

auto gse::webgpu::commands::copy_buffer(const gpu::handle<gpu::buffer> src, const gpu::handle<gpu::buffer> dst, const gpu::buffer_copy_region& region) const -> void {
	const auto* source = record_of<buffer_record>(src);
	const auto* destination = record_of<buffer_record>(dst);
	wgpuCommandEncoderCopyBufferToBuffer(list()->encoder, source->buffer, region.src_offset, destination->buffer, region.dst_offset, region.size);
}

auto gse::webgpu::commands::fill_buffer(const gpu::handle<gpu::buffer> dst, const gpu::device_size offset, const gpu::device_size size, const std::uint32_t data) const -> void {
	if (data != 0) {
		not_implemented("fill_buffer with a non-zero value");
	}
	auto* destination = record_of<buffer_record>(dst);
	wgpuCommandEncoderClearBuffer(list()->encoder, destination->buffer, offset, size == gpu::whole_size ? destination->size - offset : size);
}

auto gse::webgpu::commands::copy_buffer_to_image(const gpu::handle<gpu::buffer> src, const gpu::handle<gpu::image> dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	auto* source = record_of<buffer_record>(src);
	auto* texture = record_of<texture_record>(dst);
	assert(source->shadow != nullptr, "webgpu: buffer-to-image copies need a host-visible source until the staging slice lands");
	for (const auto& region : regions) {
		list()->owner->write_texture_region(*texture, source->shadow.get() + region.buffer_offset, region);
	}
}

auto gse::webgpu::commands::copy_image_to_buffer(const gpu::handle<gpu::image> src, const gpu::handle<gpu::buffer> dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	const auto* texture = record_of<texture_record>(src);
	auto* destination = record_of<buffer_record>(dst);
	auto* cmd_list = list();
	for (const auto& region : regions) {
		const auto row_texels = region.buffer_row_length != 0 ? region.buffer_row_length : region.image_extent.x();
		const auto image_rows = region.buffer_image_height != 0 ? region.buffer_image_height : region.image_extent.y();
		const WGPUTexelCopyTextureInfo source{
			.texture = texture->texture,
			.mipLevel = region.image_subresource.mip_level,
			.origin = { .x = static_cast<std::uint32_t>(region.image_offset.x()), .y = static_cast<std::uint32_t>(region.image_offset.y()), .z = static_cast<std::uint32_t>(region.image_offset.z()) + region.image_subresource.base_array_layer },
			.aspect = WGPUTextureAspect_All,
		};
		const WGPUTexelCopyBufferInfo target{
			.layout = {
				.offset = region.buffer_offset,
				.bytesPerRow = (row_texels * gpu::bytes_per_pixel(texture->format) + 255) & ~std::uint32_t{ 255 },
				.rowsPerImage = image_rows,
			},
			.buffer = destination->buffer,
		};
		const WGPUExtent3D size{
			.width = region.image_extent.x(),
			.height = region.image_extent.y(),
			.depthOrArrayLayers = std::max(region.image_extent.z(), region.image_subresource.layer_count),
		};
		wgpuCommandEncoderCopyTextureToBuffer(cmd_list->encoder, &source, &target, &size);
	}
	if (!std::ranges::contains(cmd_list->readbacks, destination)) {
		cmd_list->readbacks.push_back(destination);
	}
}

auto gse::webgpu::commands::blit_image(const gpu::handle<gpu::image> src, const gpu::handle<gpu::image> dst, const gpu::image_blit_region& region, gpu::sampler_filter) const -> void {
	const auto* source = record_of<texture_record>(src);
	const auto* destination = record_of<texture_record>(dst);
	const auto src_size = region.src_offsets[1] - region.src_offsets[0];
	const auto dst_size = region.dst_offsets[1] - region.dst_offsets[0];
	if (source->format != destination->format || src_size != dst_size) {
		not_implemented("blit_image with a scale or format change");
	}
	copy_image(src, dst, {
		.src_subresource = region.src_subresource,
		.src_offset = region.src_offsets[0],
		.dst_subresource = region.dst_subresource,
		.dst_offset = region.dst_offsets[0],
		.extent = vec3u{ static_cast<std::uint32_t>(src_size.x()), static_cast<std::uint32_t>(src_size.y()), static_cast<std::uint32_t>(std::max(src_size.z(), 1)) },
	});
}

auto gse::webgpu::commands::copy_image(const gpu::handle<gpu::image> src, const gpu::handle<gpu::image> dst, const gpu::image_copy_region& region) const -> void {
	const WGPUTexelCopyTextureInfo source{
		.texture = record_of<texture_record>(src)->texture,
		.mipLevel = region.src_subresource.mip_level,
		.origin = { .x = static_cast<std::uint32_t>(region.src_offset.x()), .y = static_cast<std::uint32_t>(region.src_offset.y()), .z = region.src_subresource.base_array_layer },
		.aspect = WGPUTextureAspect_All,
	};
	const WGPUTexelCopyTextureInfo destination{
		.texture = record_of<texture_record>(dst)->texture,
		.mipLevel = region.dst_subresource.mip_level,
		.origin = { .x = static_cast<std::uint32_t>(region.dst_offset.x()), .y = static_cast<std::uint32_t>(region.dst_offset.y()), .z = region.dst_subresource.base_array_layer },
		.aspect = WGPUTextureAspect_All,
	};
	const WGPUExtent3D size{
		.width = region.extent.x(),
		.height = region.extent.y(),
		.depthOrArrayLayers = std::max(region.extent.z(), region.src_subresource.layer_count),
	};
	wgpuCommandEncoderCopyTextureToTexture(list()->encoder, &source, &destination, &size);
}

auto gse::webgpu::commands::pipeline_barrier(const gpu::dependency_info&) const -> void {
}

auto gse::webgpu::commands::build_acceleration_structures(const gpu::acceleration_structure_build_geometry_info&, std::span<const gpu::acceleration_structure_build_range_info* const>) const -> void {
	not_implemented("build_acceleration_structures");
}
