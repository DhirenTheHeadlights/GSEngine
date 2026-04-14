export module gse.platform:video_encoder;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_image;
import :gpu_device;
import :vulkan_runtime;
import :vulkan_allocator;

import gse.utility;
import gse.math;
import gse.log;

export namespace gse::gpu {
	enum class video_codec : std::uint8_t { av1, h265 };

	struct encode_capabilities {
		bool available = false;
		video_codec codec = video_codec::av1;
		vec2u max_extent{};
		vk::ExtensionProperties std_header_version{};
	};

	class video_encoder final : public non_copyable {
	public:
		video_encoder() = default;

		video_encoder(
			video_encoder&&
		) noexcept = default;

		auto operator=(
			video_encoder&&
		) noexcept -> video_encoder& = default;

		static auto probe(
			device& dev
		) -> encode_capabilities;

		static auto create(
			device& dev,
			vec2u extent,
			const encode_capabilities& probe_caps
		) -> video_encoder;

		auto encode_frame(
			std::uint32_t frame_slot,
			const image& y_plane,
			const image& uv_plane
		) -> void;

		auto wait(
			std::uint32_t frame_slot
		) -> void;

		explicit operator bool() const;
	private:
		struct per_frame {
			vk::raii::CommandPool pool = nullptr;
			vk::raii::CommandBuffer cmd = nullptr;
			vk::raii::Fence fence = nullptr;
			vulkan::buffer_resource bitstream;
			vk::Image nv12_image = nullptr;
			vk::raii::ImageView nv12_view = nullptr;
			vk::DeviceMemory nv12_memory = nullptr;
			bool submitted = false;
		};

		struct dpb_slot {
			vk::Image image = nullptr;
			vk::raii::ImageView view = nullptr;
			vk::DeviceMemory memory = nullptr;
		};

		vk::raii::VideoSessionKHR m_session = nullptr;
		vk::raii::VideoSessionParametersKHR m_params = nullptr;
		std::vector<vk::DeviceMemory> m_session_memory;
		per_frame_resource<per_frame> m_slots;
		per_frame_resource<dpb_slot> m_dpb;
		video_codec m_codec = video_codec::h265;
		vec2u m_extent{};
		std::uint64_t m_frame_number = 0;
		std::uint32_t m_gop_size = 60;
		device* m_device = nullptr;
	};
}

namespace gse::gpu {
	struct profile_chain {
		vk::VideoProfileInfoKHR profile;
		vk::VideoEncodeUsageInfoKHR usage;
		vk::VideoEncodeH265ProfileInfoKHR h265_profile;
		vk::VideoEncodeAV1ProfileInfoKHR av1_profile;
	};

	template <typename To, typename From>
	constexpr auto vk_enum(
		From v
	) -> To;

	auto build_profile(
		profile_chain& chain,
		video_codec codec
	) -> void;

	constexpr vk::DeviceSize bitstream_buffer_size = 4 * 1024 * 1024;
	constexpr auto nv12_format = vk::Format::eG8B8R82Plane420Unorm;

	constexpr vk::ImageSubresourceRange color_subresource_range{
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	auto create_nv12_image(
		const vk::raii::Device& device,
		const vk::raii::PhysicalDevice& physical_device,
		vec2u extent,
		vk::ImageUsageFlags usage,
		const vk::VideoProfileListInfoKHR& profile_list
	) -> std::tuple<vk::Image, vk::raii::ImageView, vk::DeviceMemory>;

	auto find_memory_type(
		const vk::raii::PhysicalDevice& physical_device,
		std::uint32_t type_bits,
		vk::MemoryPropertyFlags properties
	) -> std::uint32_t;
}

auto gse::gpu::video_encoder::probe(device& dev) -> encode_capabilities {
	if (!dev.queue_config().video_encode_family_index.has_value()) {
		return {};
	}

	const auto& physical = dev.physical_device();

	for (const auto codec : { video_codec::av1, video_codec::h265 }) {
		profile_chain chain{};
		build_profile(chain, codec);

		try {
			vk::VideoCapabilitiesKHR caps;
			if (codec == video_codec::av1) {
				caps = physical.getVideoCapabilitiesKHR<
					vk::VideoCapabilitiesKHR,
					vk::VideoEncodeCapabilitiesKHR,
					vk::VideoEncodeAV1CapabilitiesKHR
				>(chain.profile).get<vk::VideoCapabilitiesKHR>();
			} else {
				caps = physical.getVideoCapabilitiesKHR<
					vk::VideoCapabilitiesKHR,
					vk::VideoEncodeCapabilitiesKHR,
					vk::VideoEncodeH265CapabilitiesKHR
				>(chain.profile).get<vk::VideoCapabilitiesKHR>();
			}

			const auto codec_name = codec == video_codec::av1 ? "AV1" : "H.265";
			log::println(log::category::vulkan, "Video encode probe: {} supported (max {}x{})", codec_name, caps.maxCodedExtent.width, caps.maxCodedExtent.height);

			return {
				.available = true,
				.codec = codec,
				.max_extent = { caps.maxCodedExtent.width, caps.maxCodedExtent.height },
				.std_header_version = caps.stdHeaderVersion
			};
		}
		catch (const vk::SystemError&) {
			continue;
		}
	}

	log::println(log::category::vulkan, "Video encode probe: no supported codec found");
	return {};
}

auto gse::gpu::video_encoder::create(device& dev, const vec2u extent, const encode_capabilities& probe_caps) -> video_encoder {
	video_encoder enc;
	enc.m_device = &dev;
	enc.m_codec = probe_caps.codec;
	enc.m_extent = extent;

	profile_chain chain{};
	build_profile(chain, probe_caps.codec);
	const auto& vk_dev = dev.logical_device();
	const auto& physical = dev.physical_device();
	const auto encode_family = dev.queue_config().video_encode_family_index.value();

	vk::VideoProfileListInfoKHR profile_list{
		.profileCount = 1,
		.pProfiles = &chain.profile
	};

	auto std_header_version = probe_caps.std_header_version;
	enc.m_session = vk_dev.createVideoSessionKHR({
		.queueFamilyIndex = encode_family,
		.pVideoProfile = &chain.profile,
		.pictureFormat = nv12_format,
		.maxCodedExtent = { extent.x(), extent.y() },
		.referencePictureFormat = nv12_format,
		.maxDpbSlots = 2,
		.maxActiveReferencePictures = 1,
		.pStdHeaderVersion = &std_header_version
	});

	for (const auto& req : enc.m_session.getMemoryRequirements()) {
		const auto mem_type = find_memory_type(physical, req.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		auto mem = (*vk_dev).allocateMemory({
			.allocationSize = req.memoryRequirements.size,
			.memoryTypeIndex = mem_type
		});

		enc.m_session.bindMemory(vk::BindVideoSessionMemoryInfoKHR{
			.memoryBindIndex = req.memoryBindIndex,
			.memory = mem,
			.memoryOffset = 0,
			.memorySize = req.memoryRequirements.size
		});
		enc.m_session_memory.push_back(mem);
	}

	if (probe_caps.codec == video_codec::h265) {
		static vk::video::H265DecPicBufMgr dpb_mgr{};
		dpb_mgr.max_dec_pic_buffering_minus1[0] = 1;

		static vk::video::H265SequenceParameterSet sps{};
		sps.chroma_format_idc = vk::video::H265ChromaFormatIdc::e420;
		sps.pic_width_in_luma_samples = extent.x();
		sps.pic_height_in_luma_samples = extent.y();
		sps.bit_depth_luma_minus8 = 0;
		sps.bit_depth_chroma_minus8 = 0;
		sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
		sps.pDecPicBufMgr = &dpb_mgr;

		static vk::video::H265PictureParameterSet pps{};

		static vk::VideoEncodeH265SessionParametersAddInfoKHR h265_add{
			.stdSPSCount = 1,
			.pStdSPSs = sps,
			.stdPPSCount = 1,
			.pStdPPSs = pps
		};

		auto info = vk::VideoEncodeH265SessionParametersCreateInfoKHR{
			.maxStdSPSCount = 1,
			.maxStdPPSCount = 1,
			.pParametersAddInfo = &h265_add
		};

		enc.m_params = vk_dev.createVideoSessionParametersKHR({
			.pNext = &info,
			.videoSession = *enc.m_session
		});
	}
	else {
		static vk::video::AV1SequenceHeader seq_header{
			.seq_profile = vk::video::AV1Profile::eMain,
			.max_frame_width_minus_1 = static_cast<std::uint16_t>(extent.x() - 1),
			.max_frame_height_minus_1 = static_cast<std::uint16_t>(extent.y() - 1)
		};

		auto info = vk::VideoEncodeAV1SessionParametersCreateInfoKHR{
			.pStdSequenceHeader = seq_header
		};

		enc.m_params = vk_dev.createVideoSessionParametersKHR({
			.pNext = &info,
			.videoSession = *enc.m_session
		});
	}

	for (auto& [image, view, memory] : enc.m_dpb) {
		auto [img, v, mem] = create_nv12_image(vk_dev, physical, extent, vk::ImageUsageFlagBits::eVideoDecodeDpbKHR | vk::ImageUsageFlagBits::eVideoEncodeDpbKHR, profile_list);
		image = img;
		view = std::move(v);
		memory = mem;
	}

	for (auto& slot : enc.m_slots) {
		slot.pool = vk_dev.createCommandPool({
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = encode_family
		});

		auto bufs = vk_dev.allocateCommandBuffers({
			.commandPool = *slot.pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		});
		slot.cmd = std::move(bufs[0]);

		slot.fence = vk_dev.createFence({
			.flags = vk::FenceCreateFlagBits::eSignaled
		});

		slot.bitstream = dev.allocator().create_buffer({
			.size = bitstream_buffer_size,
			.usage = vk::BufferUsageFlagBits::eVideoEncodeDstKHR
		}, nullptr, "encode_bitstream");

		auto [img, v, mem] = create_nv12_image(vk_dev, physical, extent, vk::ImageUsageFlagBits::eVideoEncodeSrcKHR | vk::ImageUsageFlagBits::eTransferDst, profile_list);
		slot.nv12_image = img;
		slot.nv12_view = std::move(v);
		slot.nv12_memory = mem;
	}

	const auto codec_name = probe_caps.codec == video_codec::av1 ? "AV1" : "H.265";
	log::println(log::category::vulkan, "Video encoder created: {} {}x{}", codec_name, extent.x(), extent.y());

	return enc;
}

auto gse::gpu::video_encoder::encode_frame(const std::uint32_t frame_slot, const image& y_plane, const image& uv_plane) -> void {
	auto& slot = m_slots[frame_slot];
	const auto& vk_dev = m_device->logical_device();

	if (slot.submitted) {
		if (vk_dev.waitForFences(*slot.fence, vk::True, std::numeric_limits<std::uint64_t>::max()) != vk::Result::eSuccess) {
			return;
		}
		vk_dev.resetFences(*slot.fence);
		slot.submitted = false;
	}

	slot.cmd.reset();
	slot.cmd.begin({
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	});

	constexpr vk::ImageSubresourceLayers y_subresource{
		.aspectMask = vk::ImageAspectFlagBits::ePlane0,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	constexpr vk::ImageSubresourceLayers uv_subresource{
		.aspectMask = vk::ImageAspectFlagBits::ePlane1,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	constexpr vk::ImageSubresourceLayers src_subresource{
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	const std::array pre_barriers = {
		vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eNone,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.image = slot.nv12_image,
			.subresourceRange = color_subresource_range
		},
		vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
			.oldLayout = vk::ImageLayout::eGeneral,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.image = y_plane.native().image,
			.subresourceRange = color_subresource_range
		},
		vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
			.oldLayout = vk::ImageLayout::eGeneral,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.image = uv_plane.native().image,
			.subresourceRange = color_subresource_range
		}
	};
	slot.cmd.pipelineBarrier2({
		.imageMemoryBarrierCount = static_cast<std::uint32_t>(pre_barriers.size()),
		.pImageMemoryBarriers = pre_barriers.data()
	});

	slot.cmd.copyImage(y_plane.native().image, vk::ImageLayout::eTransferSrcOptimal, slot.nv12_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageCopy{
		.srcSubresource = src_subresource,
		.dstSubresource = y_subresource,
		.extent = { m_extent.x(), m_extent.y(), 1 }
	});

	slot.cmd.copyImage(uv_plane.native().image, vk::ImageLayout::eTransferSrcOptimal, slot.nv12_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageCopy{
		.srcSubresource = src_subresource,
		.dstSubresource = uv_subresource,
		.extent = { m_extent.x() / 2, m_extent.y() / 2, 1 }
	});

	const vk::ImageMemoryBarrier2 to_encode{
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eVideoEncodeKHR,
		.dstAccessMask = vk::AccessFlagBits2::eVideoEncodeReadKHR,
		.oldLayout = vk::ImageLayout::eTransferDstOptimal,
		.newLayout = vk::ImageLayout::eVideoEncodeSrcKHR,
		.image = slot.nv12_image,
		.subresourceRange = color_subresource_range
	};
	slot.cmd.pipelineBarrier2({
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &to_encode
	});

	const bool is_keyframe = (m_frame_number % m_gop_size) == 0;
	const auto dpb_index = static_cast<std::uint32_t>(m_frame_number % 2);

	vk::VideoPictureResourceInfoKHR src_picture{
		.codedExtent = { m_extent.x(), m_extent.y() },
		.baseArrayLayer = 0,
		.imageViewBinding = *slot.nv12_view
	};

	vk::VideoPictureResourceInfoKHR dpb_picture{
		.codedExtent = { m_extent.x(), m_extent.y() },
		.baseArrayLayer = 0,
		.imageViewBinding = *m_dpb[dpb_index].view
	};

	vk::VideoReferenceSlotInfoKHR setup_ref{
		.slotIndex = static_cast<std::int32_t>(dpb_index),
		.pPictureResource = &dpb_picture
	};

	std::array ref_slots = { setup_ref };

	slot.cmd.beginVideoCodingKHR({
		.videoSession = *m_session,
		.videoSessionParameters = *m_params,
		.referenceSlotCount = static_cast<std::uint32_t>(ref_slots.size()),
		.pReferenceSlots = ref_slots.data()
	});

	if (m_frame_number == 0) {
		slot.cmd.controlVideoCodingKHR({
			.flags = vk::VideoCodingControlFlagBitsKHR::eReset
		});
	}

	if (m_codec == video_codec::h265) {
		static vk::video::EncodeH265PictureInfo std_pic_info{};
		std_pic_info.pic_type = is_keyframe ? vk::video::H265PictureType::eIdr : vk::video::H265PictureType::eP;
		std_pic_info.PicOrderCntVal = static_cast<std::int32_t>(m_frame_number & 0xFF);

		static vk::video::EncodeH265SliceSegmentHeader slice_header{};

		static vk::VideoEncodeH265NaluSliceSegmentInfoKHR nalu{
			.pStdSliceSegmentHeader = slice_header
		};

		static vk::VideoEncodeH265PictureInfoKHR h265_pic{
			.naluSliceSegmentEntryCount = 1,
			.pNaluSliceSegmentEntries = &nalu,
			.pStdPictureInfo = std_pic_info
		};

		slot.cmd.encodeVideoKHR({
			.pNext = &h265_pic,
			.dstBuffer = slot.bitstream.buffer,
			.dstBufferOffset = 0,
			.dstBufferRange = bitstream_buffer_size,
			.srcPictureResource = src_picture,
			.pSetupReferenceSlot = &setup_ref
		});
	}
	else {
		static vk::video::EncodeAV1PictureInfo std_pic_info{};
		std_pic_info.frame_type = is_keyframe ? vk::video::AV1FrameType::eKey : vk::video::AV1FrameType::eInter;
		std_pic_info.primary_ref_frame = is_keyframe ? 7 : 0;

		static vk::VideoEncodeAV1PictureInfoKHR av1_pic{
			.pStdPictureInfo = std_pic_info
		};

		slot.cmd.encodeVideoKHR({
			.pNext = &av1_pic,
			.dstBuffer = slot.bitstream.buffer,
			.dstBufferOffset = 0,
			.dstBufferRange = bitstream_buffer_size,
			.srcPictureResource = src_picture,
			.pSetupReferenceSlot = &setup_ref
		});
	}

	slot.cmd.endVideoCodingKHR({});
	slot.cmd.end();

	auto submit_info = vk::CommandBufferSubmitInfo{
		.commandBuffer = *slot.cmd
	};

	std::lock_guard lock(*m_device->queue_config().mutex);
	m_device->queue_config().video_encode.submit2(vk::SubmitInfo2{
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &submit_info
	}, *slot.fence);
	slot.submitted = true;

	m_frame_number++;
}

auto gse::gpu::video_encoder::wait(const std::uint32_t frame_slot) -> void {
	auto& slot = m_slots[frame_slot];
	if (!slot.submitted) {
		return;
	}

	const auto& vk_dev = m_device->logical_device();
	std::ignore = vk_dev.waitForFences(*slot.fence, vk::True, std::numeric_limits<std::uint64_t>::max());
	vk_dev.resetFences(*slot.fence);
	slot.submitted = false;
}

gse::gpu::video_encoder::operator bool() const {
	return m_session != nullptr;
}

template <typename To, typename From>
constexpr auto gse::gpu::vk_enum(const From v) -> To {
	return static_cast<To>(static_cast<std::underlying_type_t<From>>(v));
}

auto gse::gpu::build_profile(profile_chain& chain, const video_codec codec) -> void {
	chain = {};

	chain.usage = vk::VideoEncodeUsageInfoKHR{
		.videoUsageHints = vk::VideoEncodeUsageFlagBitsKHR::eRecording,
		.videoContentHints = vk::VideoEncodeContentFlagBitsKHR::eRendered,
		.tuningMode = vk::VideoEncodeTuningModeKHR::eLowLatency
	};

	if (codec == video_codec::h265) {
		chain.h265_profile.stdProfileIdc = vk_enum<decltype(chain.h265_profile.stdProfileIdc)>(vk::video::H265ProfileIdc::eMain);
		chain.profile.pNext = &chain.h265_profile;
		chain.profile.videoCodecOperation = vk::VideoCodecOperationFlagBitsKHR::eEncodeH265;
	}
	else {
		chain.av1_profile.stdProfile = vk_enum<decltype(chain.av1_profile.stdProfile)>(vk::video::AV1Profile::eMain);
		chain.profile.pNext = &chain.av1_profile;
		chain.profile.videoCodecOperation = vk::VideoCodecOperationFlagBitsKHR::eEncodeAv1;
	}

	chain.profile.chromaSubsampling = vk::VideoChromaSubsamplingFlagBitsKHR::e420;
	chain.profile.lumaBitDepth = vk::VideoComponentBitDepthFlagBitsKHR::e8;
	chain.profile.chromaBitDepth = vk::VideoComponentBitDepthFlagBitsKHR::e8;

	chain.usage.pNext = chain.profile.pNext;
	chain.profile.pNext = &chain.usage;
}

auto gse::gpu::create_nv12_image(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, vec2u extent, vk::ImageUsageFlags usage, const vk::VideoProfileListInfoKHR& profile_list) -> std::tuple<vk::Image, vk::raii::ImageView, vk::DeviceMemory> {
	auto image = (*device).createImage({
		.pNext = &profile_list,
		.imageType = vk::ImageType::e2D,
		.format = nv12_format,
		.extent = { extent.x(), extent.y(), 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive
	});

	const auto mem_reqs = (*device).getImageMemoryRequirements(image);
	const auto mem_props = physical_device.getMemoryProperties();

	std::uint32_t mem_type = 0;
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((mem_reqs.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)) {
			mem_type = i;
			break;
		}
	}

	auto memory = (*device).allocateMemory({
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = mem_type
	});

	(*device).bindImageMemory(image, memory, 0);

	auto view = device.createImageView({
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = nv12_format,
		.subresourceRange = color_subresource_range
	});

	return { image, std::move(view), memory };
}

auto gse::gpu::find_memory_type(const vk::raii::PhysicalDevice& physical_device, std::uint32_t type_bits, vk::MemoryPropertyFlags properties) -> std::uint32_t {
	const auto mem_props = physical_device.getMemoryProperties();
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((type_bits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	return 0;
}
