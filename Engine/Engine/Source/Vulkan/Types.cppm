export module gse.vulkan:types;

import std;
import vulkan;

import gse.gpu_backend;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;


export namespace gse::vulkan {
	auto to_vk(
		gpu::cull_mode m
	) -> vk::CullModeFlagBits;

	auto to_vk(
		gpu::compare_op op
	) -> vk::CompareOp;

	auto to_vk(
		gpu::polygon_mode m
	) -> vk::PolygonMode;

	auto to_vk(
		gpu::topology t
	) -> vk::PrimitiveTopology;

	auto to_vk(
		gpu::front_face f
	) -> vk::FrontFace;

	auto to_vk(
		gpu::blend_factor f
	) -> vk::BlendFactor;

	auto to_vk(
		gpu::blend_op o
	) -> vk::BlendOp;

	auto to_vk(
		gpu::color_component_flags fls
	) -> vk::ColorComponentFlags;

	auto to_vk(
		const gpu::color_blend_equation& eq
	) -> vk::ColorBlendEquationEXT;

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
		gpu::image_format f
	) -> vk::Format;

	auto to_vk(
		gpu::image_view_type t
	) -> vk::ImageViewType;

	auto to_vk(
		gpu::index_type t
	) -> vk::IndexType;

	auto to_vk(
		gpu::bind_point p
	) -> vk::PipelineBindPoint;

	auto to_vk(
		gpu::descriptor_type t
	) -> vk::DescriptorType;

	auto to_vk(
		gpu::vertex_format f
	) -> vk::Format;

	auto to_vk(
		gpu::acceleration_structure_type t
	) -> vk::AccelerationStructureTypeKHR;

	auto to_vk(
		gpu::build_acceleration_structure_mode m
	) -> vk::BuildAccelerationStructureModeKHR;

	auto to_vk(
		gpu::result r
	) -> vk::Result;

	auto to_vk(
		gpu::present_mode m
	) -> vk::PresentModeKHR;

	auto to_vk(
		gpu::color_space c
	) -> vk::ColorSpaceKHR;

	auto to_vk(
		gpu::load_op op
	) -> vk::AttachmentLoadOp;

	auto to_vk(
		gpu::store_op op
	) -> vk::AttachmentStoreOp;

	auto to_vk(
		gpu::image_type t
	) -> vk::ImageType;

	auto to_vk(
		gpu::sample_count c
	) -> vk::SampleCountFlagBits;

	auto to_vk(
		gpu::stage_flag s
	) -> vk::ShaderStageFlagBits;

	auto to_vk(
		gpu::buffer_usage fls
	) -> vk::BufferUsageFlags;

	auto to_vk(
		gpu::image_usage fls
	) -> vk::ImageUsageFlags;

	auto to_vk(
		gpu::stage_flags fls
	) -> vk::ShaderStageFlags;

	auto to_vk(
		gpu::build_acceleration_structure_flags fls
	) -> vk::BuildAccelerationStructureFlagsKHR;

	auto to_vk(
		gpu::geometry_flags fls
	) -> vk::GeometryFlagsKHR;

	auto to_vk(
		gpu::image_aspect_flags fls
	) -> vk::ImageAspectFlags;

	auto to_vk(
		gpu::access_flags fls
	) -> vk::AccessFlags2;

	auto to_vk(
		gpu::pipeline_stage_flags fls
	) -> vk::PipelineStageFlags2;

	auto to_vk(
		gpu::pipeline_statistic_flags fls
	) -> vk::QueryPipelineStatisticFlags;

	auto format_value(
		gpu::image_format f
	) -> gpu::image_format_value;

	auto image_aspect_for(
		gpu::image_format_value f
	) -> gpu::image_aspect_flags;

	auto to_vk(
		gpu::memory_property_flags fls
	) -> vk::MemoryPropertyFlags;

	auto to_vk(
		gpu::image_create_flags fls
	) -> vk::ImageCreateFlags;

	auto to_vk(
		const gpu::surface_format& sf
	) -> vk::SurfaceFormatKHR;

	auto to_vk(
		const gpu::viewport& v
	) -> vk::Viewport;

	auto to_vk(
		const gpu::push_constant_range& pcr
	) -> vk::PushConstantRange;

	auto to_vk(
		const gpu::buffer_copy_region& r
	) -> vk::BufferCopy;

	auto to_vk(
		const gpu::image_subresource_layers& s
	) -> vk::ImageSubresourceLayers;

	auto to_vk(
		const gpu::buffer_image_copy_region& r
	) -> vk::BufferImageCopy;

	auto to_vk(
		const gpu::image_copy_region& r
	) -> vk::ImageCopy;

	auto to_vk(
		const gpu::image_blit_region& r
	) -> vk::ImageBlit;

	auto to_vk(
		const gpu::acceleration_structure_build_range_info& r
	) -> vk::AccelerationStructureBuildRangeInfoKHR;

	auto from_vk(
		vk::Result r
	) -> gpu::result;

	auto from_vk(
		vk::PresentModeKHR mode
	) -> gpu::present_mode;

	auto from_vk(
		vk::ColorSpaceKHR cs
	) -> gpu::color_space;

	auto from_vk(
		vk::Format f
	) -> gpu::image_format;

	auto from_vk(
		const vk::SurfaceFormatKHR& sf
	) -> gpu::surface_format;

	auto from_vk(
		const vk::SurfaceCapabilitiesKHR& caps
	) -> gpu::surface_capabilities;
}

auto gse::vulkan::to_vk(const gpu::cull_mode m) -> vk::CullModeFlagBits {
	switch (m) {
		case gpu::cull_mode::none:
			return vk::CullModeFlagBits::eNone;
		case gpu::cull_mode::front:
			return vk::CullModeFlagBits::eFront;
		case gpu::cull_mode::back:
			return vk::CullModeFlagBits::eBack;
	}
	std::unreachable();
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
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::polygon_mode m) -> vk::PolygonMode {
	switch (m) {
		case gpu::polygon_mode::fill:
			return vk::PolygonMode::eFill;
		case gpu::polygon_mode::line:
			return vk::PolygonMode::eLine;
		case gpu::polygon_mode::point:
			return vk::PolygonMode::ePoint;
	}
	std::unreachable();
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
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::front_face f) -> vk::FrontFace {
	switch (f) {
		case gpu::front_face::counter_clockwise:
			return vk::FrontFace::eCounterClockwise;
		case gpu::front_face::clockwise:
			return vk::FrontFace::eClockwise;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::blend_factor f) -> vk::BlendFactor {
	switch (f) {
		case gpu::blend_factor::zero:
			return vk::BlendFactor::eZero;
		case gpu::blend_factor::one:
			return vk::BlendFactor::eOne;
		case gpu::blend_factor::src_color:
			return vk::BlendFactor::eSrcColor;
		case gpu::blend_factor::one_minus_src_color:
			return vk::BlendFactor::eOneMinusSrcColor;
		case gpu::blend_factor::dst_color:
			return vk::BlendFactor::eDstColor;
		case gpu::blend_factor::one_minus_dst_color:
			return vk::BlendFactor::eOneMinusDstColor;
		case gpu::blend_factor::src_alpha:
			return vk::BlendFactor::eSrcAlpha;
		case gpu::blend_factor::one_minus_src_alpha:
			return vk::BlendFactor::eOneMinusSrcAlpha;
		case gpu::blend_factor::dst_alpha:
			return vk::BlendFactor::eDstAlpha;
		case gpu::blend_factor::one_minus_dst_alpha:
			return vk::BlendFactor::eOneMinusDstAlpha;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::blend_op o) -> vk::BlendOp {
	switch (o) {
		case gpu::blend_op::add:
			return vk::BlendOp::eAdd;
		case gpu::blend_op::subtract:
			return vk::BlendOp::eSubtract;
		case gpu::blend_op::reverse_subtract:
			return vk::BlendOp::eReverseSubtract;
		case gpu::blend_op::min:
			return vk::BlendOp::eMin;
		case gpu::blend_op::max:
			return vk::BlendOp::eMax;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::color_component_flags fls) -> vk::ColorComponentFlags {
	vk::ColorComponentFlags result{};
	if (fls.test(gpu::color_component_flag::r)) {
		result |= vk::ColorComponentFlagBits::eR;
	}
	if (fls.test(gpu::color_component_flag::g)) {
		result |= vk::ColorComponentFlagBits::eG;
	}
	if (fls.test(gpu::color_component_flag::b)) {
		result |= vk::ColorComponentFlagBits::eB;
	}
	if (fls.test(gpu::color_component_flag::a)) {
		result |= vk::ColorComponentFlagBits::eA;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::color_blend_equation& eq) -> vk::ColorBlendEquationEXT {
	return vk::ColorBlendEquationEXT{
		.srcColorBlendFactor = to_vk(eq.src_color),
		.dstColorBlendFactor = to_vk(eq.dst_color),
		.colorBlendOp = to_vk(eq.color_op),
		.srcAlphaBlendFactor = to_vk(eq.src_alpha),
		.dstAlphaBlendFactor = to_vk(eq.dst_alpha),
		.alphaBlendOp = to_vk(eq.alpha_op),
	};
}

auto gse::vulkan::to_vk(const gpu::sampler_filter f) -> vk::Filter {
	switch (f) {
		case gpu::sampler_filter::nearest:
			return vk::Filter::eNearest;
		case gpu::sampler_filter::linear:
			return vk::Filter::eLinear;
	}
	std::unreachable();
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
	std::unreachable();
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
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::image_format f) -> vk::Format {
	switch (f) {
		case gpu::image_format::d32_sfloat:
			return vk::Format::eD32Sfloat;
		case gpu::image_format::r8g8b8a8_srgb:
			return vk::Format::eR8G8B8A8Srgb;
		case gpu::image_format::r8g8b8a8_unorm:
			return vk::Format::eR8G8B8A8Unorm;
		case gpu::image_format::b8g8r8a8_srgb:
			return vk::Format::eB8G8R8A8Srgb;
		case gpu::image_format::b8g8r8a8_unorm:
			return vk::Format::eB8G8R8A8Unorm;
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
		case gpu::image_format::r16g16_sfloat:
			return vk::Format::eR16G16Sfloat;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::image_view_type t) -> vk::ImageViewType {
	switch (t) {
		case gpu::image_view_type::e2d:
			return vk::ImageViewType::e2D;
		case gpu::image_view_type::e3d:
			return vk::ImageViewType::e3D;
		case gpu::image_view_type::cube:
			return vk::ImageViewType::eCube;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::index_type t) -> vk::IndexType {
	switch (t) {
		case gpu::index_type::uint16:
			return vk::IndexType::eUint16;
		case gpu::index_type::uint32:
			return vk::IndexType::eUint32;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::bind_point p) -> vk::PipelineBindPoint {
	switch (p) {
		case gpu::bind_point::graphics:
			return vk::PipelineBindPoint::eGraphics;
		case gpu::bind_point::compute:
			return vk::PipelineBindPoint::eCompute;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::descriptor_type t) -> vk::DescriptorType {
	switch (t) {
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
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::vertex_format f) -> vk::Format {
	switch (f) {
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
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::acceleration_structure_type t) -> vk::AccelerationStructureTypeKHR {
	switch (t) {
		case gpu::acceleration_structure_type::top_level:
			return vk::AccelerationStructureTypeKHR::eTopLevel;
		case gpu::acceleration_structure_type::bottom_level:
			return vk::AccelerationStructureTypeKHR::eBottomLevel;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::build_acceleration_structure_mode m) -> vk::BuildAccelerationStructureModeKHR {
	switch (m) {
		case gpu::build_acceleration_structure_mode::build:
			return vk::BuildAccelerationStructureModeKHR::eBuild;
		case gpu::build_acceleration_structure_mode::update:
			return vk::BuildAccelerationStructureModeKHR::eUpdate;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::result r) -> vk::Result {
	switch (r) {
		case gpu::result::success:
			return vk::Result::eSuccess;
		case gpu::result::not_ready:
			return vk::Result::eNotReady;
		case gpu::result::timeout:
			return vk::Result::eTimeout;
		case gpu::result::event_set:
			return vk::Result::eEventSet;
		case gpu::result::event_reset:
			return vk::Result::eEventReset;
		case gpu::result::incomplete:
			return vk::Result::eIncomplete;
		case gpu::result::suboptimal_khr:
			return vk::Result::eSuboptimalKHR;
		case gpu::result::error_out_of_host_memory:
			return vk::Result::eErrorOutOfHostMemory;
		case gpu::result::error_out_of_device_memory:
			return vk::Result::eErrorOutOfDeviceMemory;
		case gpu::result::error_device_lost:
			return vk::Result::eErrorDeviceLost;
		case gpu::result::error_out_of_date_khr:
			return vk::Result::eErrorOutOfDateKHR;
		case gpu::result::error_surface_lost_khr:
			return vk::Result::eErrorSurfaceLostKHR;
		case gpu::result::error_unknown:
			return vk::Result::eErrorUnknown;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::present_mode m) -> vk::PresentModeKHR {
	switch (m) {
		case gpu::present_mode::immediate:
			return vk::PresentModeKHR::eImmediate;
		case gpu::present_mode::mailbox:
			return vk::PresentModeKHR::eMailbox;
		case gpu::present_mode::fifo:
			return vk::PresentModeKHR::eFifo;
		case gpu::present_mode::fifo_relaxed:
			return vk::PresentModeKHR::eFifoRelaxed;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::color_space c) -> vk::ColorSpaceKHR {
	switch (c) {
		case gpu::color_space::srgb_nonlinear:
			return vk::ColorSpaceKHR::eSrgbNonlinear;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::load_op op) -> vk::AttachmentLoadOp {
	switch (op) {
		case gpu::load_op::load:
			return vk::AttachmentLoadOp::eLoad;
		case gpu::load_op::clear:
			return vk::AttachmentLoadOp::eClear;
		case gpu::load_op::dont_care:
			return vk::AttachmentLoadOp::eDontCare;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::store_op op) -> vk::AttachmentStoreOp {
	switch (op) {
		case gpu::store_op::store:
			return vk::AttachmentStoreOp::eStore;
		case gpu::store_op::dont_care:
			return vk::AttachmentStoreOp::eDontCare;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::image_type t) -> vk::ImageType {
	switch (t) {
		case gpu::image_type::e1d:
			return vk::ImageType::e1D;
		case gpu::image_type::e2d:
			return vk::ImageType::e2D;
		case gpu::image_type::e3d:
			return vk::ImageType::e3D;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::sample_count c) -> vk::SampleCountFlagBits {
	switch (c) {
		case gpu::sample_count::e1:
			return vk::SampleCountFlagBits::e1;
		case gpu::sample_count::e2:
			return vk::SampleCountFlagBits::e2;
		case gpu::sample_count::e4:
			return vk::SampleCountFlagBits::e4;
		case gpu::sample_count::e8:
			return vk::SampleCountFlagBits::e8;
		case gpu::sample_count::e16:
			return vk::SampleCountFlagBits::e16;
		case gpu::sample_count::e32:
			return vk::SampleCountFlagBits::e32;
		case gpu::sample_count::e64:
			return vk::SampleCountFlagBits::e64;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::stage_flag s) -> vk::ShaderStageFlagBits {
	switch (s) {
		case gpu::stage_flag::vertex:
			return vk::ShaderStageFlagBits::eVertex;
		case gpu::stage_flag::fragment:
			return vk::ShaderStageFlagBits::eFragment;
		case gpu::stage_flag::compute:
			return vk::ShaderStageFlagBits::eCompute;
		case gpu::stage_flag::task:
			return vk::ShaderStageFlagBits::eTaskEXT;
		case gpu::stage_flag::mesh:
			return vk::ShaderStageFlagBits::eMeshEXT;
	}
	std::unreachable();
}

auto gse::vulkan::to_vk(const gpu::buffer_usage fls) -> vk::BufferUsageFlags {
	vk::BufferUsageFlags result{};
	if (fls.test(gpu::buffer_flag::uniform)) {
		result |= vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	if (fls.test(gpu::buffer_flag::storage)) {
		result |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	if (fls.test(gpu::buffer_flag::indirect)) {
		result |= vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	if (fls.test(gpu::buffer_flag::transfer_dst)) {
		result |= vk::BufferUsageFlagBits::eTransferDst;
	}
	if (fls.test(gpu::buffer_flag::index)) {
		result |= vk::BufferUsageFlagBits::eIndexBuffer;
	}
	if (fls.test(gpu::buffer_flag::transfer_src)) {
		result |= vk::BufferUsageFlagBits::eTransferSrc;
	}
	if (fls.test(gpu::buffer_flag::acceleration_structure_storage)) {
		result |=
			vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	if (fls.test(gpu::buffer_flag::acceleration_structure_build_input)) {
		result |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
			vk::BufferUsageFlagBits::eShaderDeviceAddress;
	}
	if (fls.test(gpu::buffer_flag::video_encode_dst)) {
		result |= vk::BufferUsageFlagBits::eVideoEncodeDstKHR;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::image_usage fls) -> vk::ImageUsageFlags {
	vk::ImageUsageFlags result{};
	if (fls.test(gpu::image_flag::sampled)) {
		result |= vk::ImageUsageFlagBits::eSampled;
	}
	if (fls.test(gpu::image_flag::depth_attachment)) {
		result |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
	}
	if (fls.test(gpu::image_flag::color_attachment)) {
		result |= vk::ImageUsageFlagBits::eColorAttachment;
	}
	if (fls.test(gpu::image_flag::transfer_dst)) {
		result |= vk::ImageUsageFlagBits::eTransferDst;
	}
	if (fls.test(gpu::image_flag::storage)) {
		result |= vk::ImageUsageFlagBits::eStorage;
	}
	if (fls.test(gpu::image_flag::transfer_src)) {
		result |= vk::ImageUsageFlagBits::eTransferSrc;
	}
	if (fls.test(gpu::image_flag::host_transfer)) {
		result |= vk::ImageUsageFlagBits::eHostTransferEXT;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::stage_flags fls) -> vk::ShaderStageFlags {
	vk::ShaderStageFlags result{};
	if (fls.test(gpu::stage_flag::vertex)) {
		result |= vk::ShaderStageFlagBits::eVertex;
	}
	if (fls.test(gpu::stage_flag::fragment)) {
		result |= vk::ShaderStageFlagBits::eFragment;
	}
	if (fls.test(gpu::stage_flag::compute)) {
		result |= vk::ShaderStageFlagBits::eCompute;
	}
	if (fls.test(gpu::stage_flag::task)) {
		result |= vk::ShaderStageFlagBits::eTaskEXT;
	}
	if (fls.test(gpu::stage_flag::mesh)) {
		result |= vk::ShaderStageFlagBits::eMeshEXT;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::build_acceleration_structure_flags fls) -> vk::BuildAccelerationStructureFlagsKHR {
	vk::BuildAccelerationStructureFlagsKHR result{};
	if (fls.test(gpu::build_acceleration_structure_flag::allow_update)) {
		result |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
	}
	if (fls.test(gpu::build_acceleration_structure_flag::allow_compaction)) {
		result |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
	}
	if (fls.test(gpu::build_acceleration_structure_flag::prefer_fast_trace)) {
		result |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	}
	if (fls.test(gpu::build_acceleration_structure_flag::prefer_fast_build)) {
		result |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild;
	}
	if (fls.test(gpu::build_acceleration_structure_flag::low_memory)) {
		result |= vk::BuildAccelerationStructureFlagBitsKHR::eLowMemory;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::geometry_flags fls) -> vk::GeometryFlagsKHR {
	vk::GeometryFlagsKHR result{};
	if (fls.test(gpu::geometry_flag::opaque)) {
		result |= vk::GeometryFlagBitsKHR::eOpaque;
	}
	if (fls.test(gpu::geometry_flag::no_duplicate_any_hit_invocation)) {
		result |= vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::image_aspect_flags fls) -> vk::ImageAspectFlags {
	vk::ImageAspectFlags result{};
	if (fls.test(gpu::image_aspect_flag::color)) {
		result |= vk::ImageAspectFlagBits::eColor;
	}
	if (fls.test(gpu::image_aspect_flag::depth)) {
		result |= vk::ImageAspectFlagBits::eDepth;
	}
	if (fls.test(gpu::image_aspect_flag::stencil)) {
		result |= vk::ImageAspectFlagBits::eStencil;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::access_flags fls) -> vk::AccessFlags2 {
	vk::AccessFlags2 result{};
	if (fls.test(gpu::access_flag::indirect_command_read)) {
		result |= vk::AccessFlagBits2::eIndirectCommandRead;
	}
	if (fls.test(gpu::access_flag::index_read)) {
		result |= vk::AccessFlagBits2::eIndexRead;
	}
	if (fls.test(gpu::access_flag::vertex_attribute_read)) {
		result |= vk::AccessFlagBits2::eVertexAttributeRead;
	}
	if (fls.test(gpu::access_flag::uniform_read)) {
		result |= vk::AccessFlagBits2::eUniformRead;
	}
	if (fls.test(gpu::access_flag::input_attachment_read)) {
		result |= vk::AccessFlagBits2::eInputAttachmentRead;
	}
	if (fls.test(gpu::access_flag::shader_read)) {
		result |= vk::AccessFlagBits2::eShaderRead;
	}
	if (fls.test(gpu::access_flag::shader_write)) {
		result |= vk::AccessFlagBits2::eShaderWrite;
	}
	if (fls.test(gpu::access_flag::color_attachment_read)) {
		result |= vk::AccessFlagBits2::eColorAttachmentRead;
	}
	if (fls.test(gpu::access_flag::color_attachment_write)) {
		result |= vk::AccessFlagBits2::eColorAttachmentWrite;
	}
	if (fls.test(gpu::access_flag::depth_stencil_attachment_read)) {
		result |= vk::AccessFlagBits2::eDepthStencilAttachmentRead;
	}
	if (fls.test(gpu::access_flag::depth_stencil_attachment_write)) {
		result |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
	}
	if (fls.test(gpu::access_flag::transfer_read)) {
		result |= vk::AccessFlagBits2::eTransferRead;
	}
	if (fls.test(gpu::access_flag::transfer_write)) {
		result |= vk::AccessFlagBits2::eTransferWrite;
	}
	if (fls.test(gpu::access_flag::host_read)) {
		result |= vk::AccessFlagBits2::eHostRead;
	}
	if (fls.test(gpu::access_flag::host_write)) {
		result |= vk::AccessFlagBits2::eHostWrite;
	}
	if (fls.test(gpu::access_flag::memory_read)) {
		result |= vk::AccessFlagBits2::eMemoryRead;
	}
	if (fls.test(gpu::access_flag::memory_write)) {
		result |= vk::AccessFlagBits2::eMemoryWrite;
	}
	if (fls.test(gpu::access_flag::shader_sampled_read)) {
		result |= vk::AccessFlagBits2::eShaderSampledRead;
	}
	if (fls.test(gpu::access_flag::shader_storage_read)) {
		result |= vk::AccessFlagBits2::eShaderStorageRead;
	}
	if (fls.test(gpu::access_flag::shader_storage_write)) {
		result |= vk::AccessFlagBits2::eShaderStorageWrite;
	}
	if (fls.test(gpu::access_flag::acceleration_structure_read)) {
		result |= vk::AccessFlagBits2::eAccelerationStructureReadKHR;
	}
	if (fls.test(gpu::access_flag::acceleration_structure_write)) {
		result |= vk::AccessFlagBits2::eAccelerationStructureWriteKHR;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::pipeline_stage_flags fls) -> vk::PipelineStageFlags2 {
	vk::PipelineStageFlags2 result{};
	if (fls.test(gpu::pipeline_stage_flag::top_of_pipe)) {
		result |= vk::PipelineStageFlagBits2::eTopOfPipe;
	}
	if (fls.test(gpu::pipeline_stage_flag::draw_indirect)) {
		result |= vk::PipelineStageFlagBits2::eDrawIndirect;
	}
	if (fls.test(gpu::pipeline_stage_flag::vertex_input)) {
		result |= vk::PipelineStageFlagBits2::eVertexInput;
	}
	if (fls.test(gpu::pipeline_stage_flag::vertex_shader)) {
		result |= vk::PipelineStageFlagBits2::eVertexShader;
	}
	if (fls.test(gpu::pipeline_stage_flag::tessellation_control)) {
		result |= vk::PipelineStageFlagBits2::eTessellationControlShader;
	}
	if (fls.test(gpu::pipeline_stage_flag::tessellation_evaluation)) {
		result |= vk::PipelineStageFlagBits2::eTessellationEvaluationShader;
	}
	if (fls.test(gpu::pipeline_stage_flag::geometry_shader)) {
		result |= vk::PipelineStageFlagBits2::eGeometryShader;
	}
	if (fls.test(gpu::pipeline_stage_flag::fragment_shader)) {
		result |= vk::PipelineStageFlagBits2::eFragmentShader;
	}
	if (fls.test(gpu::pipeline_stage_flag::early_fragment_tests)) {
		result |= vk::PipelineStageFlagBits2::eEarlyFragmentTests;
	}
	if (fls.test(gpu::pipeline_stage_flag::late_fragment_tests)) {
		result |= vk::PipelineStageFlagBits2::eLateFragmentTests;
	}
	if (fls.test(gpu::pipeline_stage_flag::color_attachment_output)) {
		result |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
	}
	if (fls.test(gpu::pipeline_stage_flag::compute_shader)) {
		result |= vk::PipelineStageFlagBits2::eComputeShader;
	}
	if (fls.test(gpu::pipeline_stage_flag::transfer)) {
		result |= vk::PipelineStageFlagBits2::eAllTransfer;
	}
	if (fls.test(gpu::pipeline_stage_flag::bottom_of_pipe)) {
		result |= vk::PipelineStageFlagBits2::eBottomOfPipe;
	}
	if (fls.test(gpu::pipeline_stage_flag::host)) {
		result |= vk::PipelineStageFlagBits2::eHost;
	}
	if (fls.test(gpu::pipeline_stage_flag::all_graphics)) {
		result |= vk::PipelineStageFlagBits2::eAllGraphics;
	}
	if (fls.test(gpu::pipeline_stage_flag::all_commands)) {
		result |= vk::PipelineStageFlagBits2::eAllCommands;
	}
	if (fls.test(gpu::pipeline_stage_flag::copy)) {
		result |= vk::PipelineStageFlagBits2::eCopy;
	}
	if (fls.test(gpu::pipeline_stage_flag::resolve)) {
		result |= vk::PipelineStageFlagBits2::eResolve;
	}
	if (fls.test(gpu::pipeline_stage_flag::blit)) {
		result |= vk::PipelineStageFlagBits2::eBlit;
	}
	if (fls.test(gpu::pipeline_stage_flag::clear)) {
		result |= vk::PipelineStageFlagBits2::eClear;
	}
	if (fls.test(gpu::pipeline_stage_flag::index_input)) {
		result |= vk::PipelineStageFlagBits2::eIndexInput;
	}
	if (fls.test(gpu::pipeline_stage_flag::vertex_attribute_input)) {
		result |= vk::PipelineStageFlagBits2::eVertexAttributeInput;
	}
	if (fls.test(gpu::pipeline_stage_flag::pre_rasterization_shaders)) {
		result |= vk::PipelineStageFlagBits2::ePreRasterizationShaders;
	}
	if (fls.test(gpu::pipeline_stage_flag::mesh_shader)) {
		result |= vk::PipelineStageFlagBits2::eMeshShaderEXT;
	}
	if (fls.test(gpu::pipeline_stage_flag::task_shader)) {
		result |= vk::PipelineStageFlagBits2::eTaskShaderEXT;
	}
	if (fls.test(gpu::pipeline_stage_flag::acceleration_structure_build)) {
		result |= vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
	}
	if (fls.test(gpu::pipeline_stage_flag::ray_tracing_shader)) {
		result |= vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::memory_property_flags fls) -> vk::MemoryPropertyFlags {
	vk::MemoryPropertyFlags result{};
	if (fls.test(gpu::memory_property_flag::device_local)) {
		result |= vk::MemoryPropertyFlagBits::eDeviceLocal;
	}
	if (fls.test(gpu::memory_property_flag::host_visible)) {
		result |= vk::MemoryPropertyFlagBits::eHostVisible;
	}
	if (fls.test(gpu::memory_property_flag::host_coherent)) {
		result |= vk::MemoryPropertyFlagBits::eHostCoherent;
	}
	if (fls.test(gpu::memory_property_flag::host_cached)) {
		result |= vk::MemoryPropertyFlagBits::eHostCached;
	}
	if (fls.test(gpu::memory_property_flag::lazily_allocated)) {
		result |= vk::MemoryPropertyFlagBits::eLazilyAllocated;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::image_create_flags fls) -> vk::ImageCreateFlags {
	vk::ImageCreateFlags result{};
	if (fls.test(gpu::image_create_flag::cube_compatible)) {
		result |= vk::ImageCreateFlagBits::eCubeCompatible;
	}
	return result;
}

auto gse::vulkan::to_vk(const gpu::surface_format& sf) -> vk::SurfaceFormatKHR {
	return {
		.format = to_vk(sf.format),
		.colorSpace = to_vk(sf.color_space),
	};
}

auto gse::vulkan::to_vk(const gpu::viewport& v) -> vk::Viewport {
	return {
		.x = v.x,
		.y = v.y,
		.width = v.width,
		.height = v.height,
		.minDepth = v.min_depth,
		.maxDepth = v.max_depth,
	};
}

auto gse::vulkan::to_vk(const gpu::push_constant_range& pcr) -> vk::PushConstantRange {
	return {
		.stageFlags = to_vk(pcr.stages),
		.offset = pcr.offset,
		.size = pcr.size,
	};
}

auto gse::vulkan::to_vk(const gpu::buffer_copy_region& r) -> vk::BufferCopy {
	return {
		.srcOffset = r.src_offset,
		.dstOffset = r.dst_offset,
		.size = r.size,
	};
}

auto gse::vulkan::to_vk(const gpu::image_subresource_layers& s) -> vk::ImageSubresourceLayers {
	return {
		.aspectMask = to_vk(s.aspects),
		.mipLevel = s.mip_level,
		.baseArrayLayer = s.base_array_layer,
		.layerCount = s.layer_count,
	};
}

auto gse::vulkan::to_vk(const gpu::buffer_image_copy_region& r) -> vk::BufferImageCopy {
	return {
		.bufferOffset = r.buffer_offset,
		.bufferRowLength = r.buffer_row_length,
		.bufferImageHeight = r.buffer_image_height,
		.imageSubresource = to_vk(r.image_subresource),
		.imageOffset = vk::Offset3D{ r.image_offset.x(), r.image_offset.y(), r.image_offset.z() },
		.imageExtent = vk::Extent3D{ r.image_extent.x(), r.image_extent.y(), r.image_extent.z() },
	};
}

auto gse::vulkan::to_vk(const gpu::image_copy_region& r) -> vk::ImageCopy {
	return {
		.srcSubresource = to_vk(r.src_subresource),
		.srcOffset = vk::Offset3D{ r.src_offset.x(), r.src_offset.y(), r.src_offset.z() },
		.dstSubresource = to_vk(r.dst_subresource),
		.dstOffset = vk::Offset3D{ r.dst_offset.x(), r.dst_offset.y(), r.dst_offset.z() },
		.extent = vk::Extent3D{ r.extent.x(), r.extent.y(), r.extent.z() },
	};
}

auto gse::vulkan::to_vk(const gpu::image_blit_region& r) -> vk::ImageBlit {
	vk::ImageBlit out{
		.srcSubresource = to_vk(r.src_subresource),
		.dstSubresource = to_vk(r.dst_subresource),
	};
	out.srcOffsets[0] = vk::Offset3D{ r.src_offsets[0].x(), r.src_offsets[0].y(), r.src_offsets[0].z() };
	out.srcOffsets[1] = vk::Offset3D{ r.src_offsets[1].x(), r.src_offsets[1].y(), r.src_offsets[1].z() };
	out.dstOffsets[0] = vk::Offset3D{ r.dst_offsets[0].x(), r.dst_offsets[0].y(), r.dst_offsets[0].z() };
	out.dstOffsets[1] = vk::Offset3D{ r.dst_offsets[1].x(), r.dst_offsets[1].y(), r.dst_offsets[1].z() };
	return out;
}

auto gse::vulkan::to_vk(const gpu::acceleration_structure_build_range_info& r) -> vk::AccelerationStructureBuildRangeInfoKHR {
	return {
		.primitiveCount = r.primitive_count,
		.primitiveOffset = r.primitive_offset,
		.firstVertex = r.first_vertex,
		.transformOffset = r.transform_offset,
	};
}

auto gse::vulkan::from_vk(const vk::Result r) -> gpu::result {
	switch (r) {
		case vk::Result::eSuccess:
			return gpu::result::success;
		case vk::Result::eNotReady:
			return gpu::result::not_ready;
		case vk::Result::eTimeout:
			return gpu::result::timeout;
		case vk::Result::eEventSet:
			return gpu::result::event_set;
		case vk::Result::eEventReset:
			return gpu::result::event_reset;
		case vk::Result::eIncomplete:
			return gpu::result::incomplete;
		case vk::Result::eSuboptimalKHR:
			return gpu::result::suboptimal_khr;
		case vk::Result::eErrorOutOfHostMemory:
			return gpu::result::error_out_of_host_memory;
		case vk::Result::eErrorOutOfDeviceMemory:
			return gpu::result::error_out_of_device_memory;
		case vk::Result::eErrorDeviceLost:
			return gpu::result::error_device_lost;
		case vk::Result::eErrorOutOfDateKHR:
			return gpu::result::error_out_of_date_khr;
		case vk::Result::eErrorSurfaceLostKHR:
			return gpu::result::error_surface_lost_khr;
		default:
			return gpu::result::error_unknown;
	}
}

auto gse::vulkan::from_vk(const vk::PresentModeKHR mode) -> gpu::present_mode {
	switch (mode) {
		case vk::PresentModeKHR::eImmediate:
			return gpu::present_mode::immediate;
		case vk::PresentModeKHR::eMailbox:
			return gpu::present_mode::mailbox;
		case vk::PresentModeKHR::eFifo:
			return gpu::present_mode::fifo;
		case vk::PresentModeKHR::eFifoRelaxed:
			return gpu::present_mode::fifo_relaxed;
		default:
			return gpu::present_mode::fifo;
	}
}

auto gse::vulkan::from_vk(const vk::ColorSpaceKHR cs) -> gpu::color_space {
	switch (cs) {
		case vk::ColorSpaceKHR::eSrgbNonlinear:
			return gpu::color_space::srgb_nonlinear;
		default:
			return gpu::color_space::srgb_nonlinear;
	}
}

auto gse::vulkan::from_vk(const vk::Format f) -> gpu::image_format {
	switch (f) {
		case vk::Format::eD32Sfloat:
			return gpu::image_format::d32_sfloat;
		case vk::Format::eR8G8B8A8Srgb:
			return gpu::image_format::r8g8b8a8_srgb;
		case vk::Format::eR8G8B8A8Unorm:
			return gpu::image_format::r8g8b8a8_unorm;
		case vk::Format::eB8G8R8A8Srgb:
			return gpu::image_format::b8g8r8a8_srgb;
		case vk::Format::eB8G8R8A8Unorm:
			return gpu::image_format::b8g8r8a8_unorm;
		case vk::Format::eR8G8B8Srgb:
			return gpu::image_format::r8g8b8_srgb;
		case vk::Format::eR8G8B8Unorm:
			return gpu::image_format::r8g8b8_unorm;
		case vk::Format::eR8Unorm:
			return gpu::image_format::r8_unorm;
		case vk::Format::eB10G11R11UfloatPack32:
			return gpu::image_format::b10g11r11_ufloat;
		case vk::Format::eR8G8Snorm:
			return gpu::image_format::r8g8_snorm;
		case vk::Format::eR8G8Unorm:
			return gpu::image_format::r8g8_unorm;
		case vk::Format::eR16G16B16A16Sfloat:
			return gpu::image_format::r16g16b16a16_sfloat;
		default:
			return gpu::image_format::r8g8b8a8_unorm;
	}
}

auto gse::vulkan::from_vk(const vk::SurfaceFormatKHR& sf) -> gpu::surface_format {
	return {
		.format = from_vk(sf.format),
		.color_space = from_vk(sf.colorSpace),
	};
}

auto gse::vulkan::from_vk(const vk::SurfaceCapabilitiesKHR& caps) -> gpu::surface_capabilities {
	return {
		.min_image_count = caps.minImageCount,
		.max_image_count = caps.maxImageCount,
		.current_extent = vec2u{ caps.currentExtent.width, caps.currentExtent.height },
		.min_image_extent = vec2u{ caps.minImageExtent.width, caps.minImageExtent.height },
		.max_image_extent = vec2u{ caps.maxImageExtent.width, caps.maxImageExtent.height },
		.max_image_array_layers = caps.maxImageArrayLayers,
	};
}

auto gse::vulkan::to_vk(const gpu::pipeline_statistic_flags fls) -> vk::QueryPipelineStatisticFlags {
	vk::QueryPipelineStatisticFlags out{};
	if (fls.test(gpu::pipeline_statistic_flag::input_assembly_vertices)) {
		out |= vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices;
	}
	if (fls.test(gpu::pipeline_statistic_flag::input_assembly_primitives)) {
		out |= vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives;
	}
	if (fls.test(gpu::pipeline_statistic_flag::clipping_invocations)) {
		out |= vk::QueryPipelineStatisticFlagBits::eClippingInvocations;
	}
	if (fls.test(gpu::pipeline_statistic_flag::fragment_shader_invocations)) {
		out |= vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;
	}
	return out;
}

auto gse::vulkan::format_value(const gpu::image_format f) -> gpu::image_format_value {
	return static_cast<gpu::image_format_value>(to_vk(f));
}

auto gse::vulkan::image_aspect_for(const gpu::image_format_value f) -> gpu::image_aspect_flags {
	if (f == static_cast<gpu::image_format_value>(vk::Format::eD32Sfloat)) {
		return gpu::image_aspect_flag::depth;
	}
	return gpu::image_aspect_flag::color;
}
