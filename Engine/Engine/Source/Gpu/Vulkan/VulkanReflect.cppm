export module gse.gpu:vulkan_reflect;

import std;
import vulkan;

import :types;

export namespace gse::vulkan {
	auto to_vk(
		gpu::buffer_usage usage
	) -> vk::BufferUsageFlags;

	auto to_vk(
		gpu::cull_mode mode
	) -> vk::CullModeFlags;

	auto to_vk(
		gpu::compare_op op
	) -> vk::CompareOp;

	auto to_vk(
		gpu::polygon_mode mode
	) -> vk::PolygonMode;

	auto to_vk(
		gpu::topology t
	) -> vk::PrimitiveTopology;

	auto to_vk(
		gpu::sampler_filter f
	) -> vk::Filter;

	auto to_vk(
		gpu::sampler_address_mode m
	) -> vk::SamplerAddressMode;

	auto to_vk(
		gpu::border_color c
	) -> vk::BorderColor;

	auto to_vk(
		gpu::image_layout l
	) -> vk::ImageLayout;

	auto to_vk(
		gpu::image_format f
	) -> vk::Format;

	auto to_vk(
		gpu::vertex_format fmt
	) -> vk::Format;

	auto to_vk(
		gpu::stage_flags flags
	) -> vk::ShaderStageFlags;

	auto to_vk(
		gpu::descriptor_type type
	) -> vk::DescriptorType;
}

auto gse::vulkan::to_vk(const gpu::buffer_usage usage) -> vk::BufferUsageFlags {
	using enum gpu::buffer_flag;
	vk::BufferUsageFlags flags;
	if (usage.test(uniform)) {
		flags |= vk::BufferUsageFlagBits::eUniformBuffer;
	}
	if (usage.test(storage)) {
		flags |= vk::BufferUsageFlagBits::eStorageBuffer;
	}
	if (usage.test(indirect)) {
		flags |= vk::BufferUsageFlagBits::eIndirectBuffer;
	}
	if (usage.test(transfer_dst)) {
		flags |= vk::BufferUsageFlagBits::eTransferDst;
	}
	if (usage.test(vertex)) {
		flags |= vk::BufferUsageFlagBits::eVertexBuffer;
	}
	if (usage.test(index)) {
		flags |= vk::BufferUsageFlagBits::eIndexBuffer;
	}
	if (usage.test(transfer_src)) {
		flags |= vk::BufferUsageFlagBits::eTransferSrc;
	}
	if (usage.test(acceleration_structure_storage)) {
		flags |= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
		       | vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	if (usage.test(acceleration_structure_build_input)) {
		flags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
		       | vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	if (usage.test(uniform) || usage.test(storage) || usage.test(indirect)) {
		flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	return flags;
}

auto gse::vulkan::to_vk(const gpu::cull_mode mode) -> vk::CullModeFlags {
	switch (mode) {
		case gpu::cull_mode::none:
			return vk::CullModeFlagBits::eNone;
		case gpu::cull_mode::front:
			return vk::CullModeFlagBits::eFront;
		case gpu::cull_mode::back:
			return vk::CullModeFlagBits::eBack;
	}
	return vk::CullModeFlagBits::eNone;
}

auto gse::vulkan::to_vk(const gpu::compare_op op) -> vk::CompareOp {
	switch (op) {
		case gpu::compare_op::never:
			return vk::CompareOp::eNever;
		case gpu::compare_op::less:
			return vk::CompareOp::eLess;
		case gpu::compare_op::equal:
			return vk::CompareOp::eEqual;
		case gpu::compare_op::less_or_equal:
			return vk::CompareOp::eLessOrEqual;
		case gpu::compare_op::greater:
			return vk::CompareOp::eGreater;
		case gpu::compare_op::not_equal:
			return vk::CompareOp::eNotEqual;
		case gpu::compare_op::greater_or_equal:
			return vk::CompareOp::eGreaterOrEqual;
		case gpu::compare_op::always:
			return vk::CompareOp::eAlways;
	}
	return vk::CompareOp::eAlways;
}

auto gse::vulkan::to_vk(const gpu::polygon_mode mode) -> vk::PolygonMode {
	switch (mode) {
		case gpu::polygon_mode::fill:
			return vk::PolygonMode::eFill;
		case gpu::polygon_mode::line:
			return vk::PolygonMode::eLine;
		case gpu::polygon_mode::point:
			return vk::PolygonMode::ePoint;
	}
	return vk::PolygonMode::eFill;
}

auto gse::vulkan::to_vk(const gpu::topology t) -> vk::PrimitiveTopology {
	switch (t) {
		case gpu::topology::triangle_list:
			return vk::PrimitiveTopology::eTriangleList;
		case gpu::topology::line_list:
			return vk::PrimitiveTopology::eLineList;
		case gpu::topology::point_list:
			return vk::PrimitiveTopology::ePointList;
	}
	return vk::PrimitiveTopology::eTriangleList;
}

auto gse::vulkan::to_vk(const gpu::sampler_filter f) -> vk::Filter {
	switch (f) {
		case gpu::sampler_filter::nearest:
			return vk::Filter::eNearest;
		case gpu::sampler_filter::linear:
			return vk::Filter::eLinear;
	}
	return vk::Filter::eLinear;
}

auto gse::vulkan::to_vk(const gpu::sampler_address_mode m) -> vk::SamplerAddressMode {
	switch (m) {
		case gpu::sampler_address_mode::repeat:
			return vk::SamplerAddressMode::eRepeat;
		case gpu::sampler_address_mode::clamp_to_edge:
			return vk::SamplerAddressMode::eClampToEdge;
		case gpu::sampler_address_mode::clamp_to_border:
			return vk::SamplerAddressMode::eClampToBorder;
		case gpu::sampler_address_mode::mirrored_repeat:
			return vk::SamplerAddressMode::eMirroredRepeat;
	}
	return vk::SamplerAddressMode::eRepeat;
}

auto gse::vulkan::to_vk(const gpu::border_color c) -> vk::BorderColor {
	switch (c) {
		case gpu::border_color::float_opaque_white:
			return vk::BorderColor::eFloatOpaqueWhite;
		case gpu::border_color::float_opaque_black:
			return vk::BorderColor::eFloatOpaqueBlack;
		case gpu::border_color::float_transparent_black:
			return vk::BorderColor::eFloatTransparentBlack;
	}
	return vk::BorderColor::eFloatOpaqueWhite;
}

auto gse::vulkan::to_vk(const gpu::image_layout l) -> vk::ImageLayout {
	switch (l) {
		case gpu::image_layout::undefined:
			return vk::ImageLayout::eUndefined;
		case gpu::image_layout::general:
			return vk::ImageLayout::eGeneral;
		case gpu::image_layout::shader_read_only:
			return vk::ImageLayout::eShaderReadOnlyOptimal;
	}
	return vk::ImageLayout::eUndefined;
}

auto gse::vulkan::to_vk(const gpu::image_format f) -> vk::Format {
	switch (f) {
		case gpu::image_format::d32_sfloat:
			return vk::Format::eD32Sfloat;
		case gpu::image_format::r8g8b8a8_srgb:
			return vk::Format::eR8G8B8A8Srgb;
		case gpu::image_format::r8g8b8a8_unorm:
			return vk::Format::eR8G8B8A8Unorm;
		case gpu::image_format::r8g8b8_srgb:
			return vk::Format::eR8G8B8Srgb;
		case gpu::image_format::r8g8b8_unorm:
			return vk::Format::eR8G8B8Unorm;
		case gpu::image_format::r8_unorm:
			return vk::Format::eR8Unorm;
		case gpu::image_format::b10g11r11_ufloat:
			return vk::Format::eB10G11R11UfloatPack32;
		case gpu::image_format::r8g8_snorm:
			return vk::Format::eR8G8Snorm;
		case gpu::image_format::r8g8_unorm:
			return vk::Format::eR8G8Unorm;
		case gpu::image_format::r16g16b16a16_sfloat:
			return vk::Format::eR16G16B16A16Sfloat;
	}
	return vk::Format::eD32Sfloat;
}

auto gse::vulkan::to_vk(const gpu::vertex_format fmt) -> vk::Format {
	switch (fmt) {
		case gpu::vertex_format::r32_sfloat:
			return vk::Format::eR32Sfloat;
		case gpu::vertex_format::r32g32_sfloat:
			return vk::Format::eR32G32Sfloat;
		case gpu::vertex_format::r32g32b32_sfloat:
			return vk::Format::eR32G32B32Sfloat;
		case gpu::vertex_format::r32g32b32a32_sfloat:
			return vk::Format::eR32G32B32A32Sfloat;
		case gpu::vertex_format::r32_sint:
			return vk::Format::eR32Sint;
		case gpu::vertex_format::r32g32_sint:
			return vk::Format::eR32G32Sint;
		case gpu::vertex_format::r32g32b32_sint:
			return vk::Format::eR32G32B32Sint;
		case gpu::vertex_format::r32g32b32a32_sint:
			return vk::Format::eR32G32B32A32Sint;
		case gpu::vertex_format::r32_uint:
			return vk::Format::eR32Uint;
		case gpu::vertex_format::r32g32_uint:
			return vk::Format::eR32G32Uint;
		case gpu::vertex_format::r32g32b32_uint:
			return vk::Format::eR32G32B32Uint;
		case gpu::vertex_format::r32g32b32a32_uint:
			return vk::Format::eR32G32B32A32Uint;
	}
	return vk::Format::eUndefined;
}

auto gse::vulkan::to_vk(const gpu::stage_flags flags) -> vk::ShaderStageFlags {
	using enum gpu::stage_flag;
	vk::ShaderStageFlags result;
	if (flags.test(vertex)) {
		result |= vk::ShaderStageFlagBits::eVertex;
	}
	if (flags.test(fragment)) {
		result |= vk::ShaderStageFlagBits::eFragment;
	}
	if (flags.test(compute)) {
		result |= vk::ShaderStageFlagBits::eCompute;
	}
	if (flags.test(task)) {
		result |= vk::ShaderStageFlagBits::eTaskEXT;
	}
	if (flags.test(mesh)) {
		result |= vk::ShaderStageFlagBits::eMeshEXT;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::descriptor_type type) -> vk::DescriptorType {
	switch (type) {
		case gpu::descriptor_type::uniform_buffer:
			return vk::DescriptorType::eUniformBuffer;
		case gpu::descriptor_type::storage_buffer:
			return vk::DescriptorType::eStorageBuffer;
		case gpu::descriptor_type::combined_image_sampler:
			return vk::DescriptorType::eCombinedImageSampler;
		case gpu::descriptor_type::sampled_image:
			return vk::DescriptorType::eSampledImage;
		case gpu::descriptor_type::storage_image:
			return vk::DescriptorType::eStorageImage;
		case gpu::descriptor_type::sampler:
			return vk::DescriptorType::eSampler;
		case gpu::descriptor_type::acceleration_structure:
			return vk::DescriptorType::eAccelerationStructureKHR;
	}
	return vk::DescriptorType::eStorageBuffer;
}
