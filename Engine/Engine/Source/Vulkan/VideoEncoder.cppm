export module gse.vulkan:video_encoder;

import std;
import vulkan;
import vulkan_video;

import :types;
import gse.gpu_backend;
import :device;
import :physical_device;
import :queues;

import gse.assert;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;
import gse.log;

namespace gse::vulkan {
	constexpr std::size_t source_ring_size = 3;
	constexpr std::uint64_t encode_source_lag = 2;
}

export namespace gse::vulkan {
	class video_encoder final : public non_copyable {
	public:
		video_encoder() {}
		video_encoder(
			video_encoder&&
		) noexcept = default;

		auto operator=(
			video_encoder&&
		) noexcept -> video_encoder& = default;

		static auto probe(
			device& dev,
			queue& q
		) -> gpu::encode_capabilities;

		static auto create(
			device& dev,
			queue& q,
			const gpu::encode_desc& desc,
			const gpu::encode_capabilities& probe_caps
		) -> video_encoder;

		auto set_bitrate(
			bitrate rate
		) -> void;

		[[nodiscard]] auto begin_capture(
			time pts
		) -> gpu::encode_source;

		[[nodiscard]] auto take_bitstream() -> std::optional<gpu::encoded_unit>;

		auto submit_ready() -> void;

		[[nodiscard]] auto stream_header() const -> std::span<const std::byte>;

		[[nodiscard]] auto codec() const -> gpu::video_codec;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto valid() const -> bool;

	private:
		struct per_frame {
			vk::raii::CommandPool pool = nullptr;
			vk::raii::CommandBuffer cmd = nullptr;
			vk::raii::Fence fence = nullptr;
			vk::raii::QueryPool query_pool = nullptr;
			vk::raii::QueryPool timestamp_pool = nullptr;
			gpu::buffer bitstream;
			vk::Image nv12_image = nullptr;
			vk::raii::ImageView nv12_view = nullptr;
			vk::DeviceMemory nv12_memory = nullptr;
			gpu::image y_staging;
			gpu::image uv_staging;
			gpu::bindless_handle y_plane_slot;
			gpu::bindless_handle uv_plane_slot;
			time capture_pts{};
			time last_pts{};
			trace::tick_step cpu_ref{};
			std::uint64_t timestamp_frame = 0;
			bool captured = false;
			bool last_was_keyframe = false;
			bool submitted = false;
			bool has_output = false;
			bool timestamps_pending = false;
		};

		struct dpb_slot {
			vk::Image image = nullptr;
			vk::raii::ImageView view = nullptr;
			vk::DeviceMemory memory = nullptr;
			bool active = false;
			vk::video::AV1FrameType av1_frame_type = vk::video::AV1FrameType::eKey;
			std::uint8_t av1_order_hint = 0;
			vk::video::H265PictureType h265_pic_type = vk::video::H265PictureType::eIdr;
			std::int32_t h265_poc = 0;
		};

		[[nodiscard]] auto timestamps_supported() const -> bool;

		auto publish_encode_timestamps(
			per_frame& slot
		) -> void;

		auto record_rate_control(
			per_frame& slot,
			bool reset
		) -> void;

		auto encode_capture(
			per_frame& slot
		) -> void;

		[[nodiscard]] auto read_slot_bitstream(
			per_frame& slot
		) -> std::optional<gpu::encoded_unit>;

		auto prime_source_layouts() -> void;

		vk::raii::VideoSessionKHR m_session = nullptr;
		vk::raii::VideoSessionParametersKHR m_params = nullptr;
		std::vector<vk::DeviceMemory> m_session_memory;
		std::array<per_frame, source_ring_size> m_slots;
		per_frame_resource<dpb_slot> m_dpb{ dpb_slot{}, dpb_slot{} };
		std::vector<std::byte> m_stream_header;
		gpu::video_codec m_codec = gpu::video_codec::h265;
		internal::vec_storage<unsigned int, 2> m_extent{};
		std::uint64_t m_capture_number = 0;
		std::uint64_t m_frame_number = 0;
		std::uint32_t m_gop_size = 60;
		std::uint64_t m_timestamp_ticks_mask = 0;
		time_t<double> m_timestamp_period_per_tick = nanoseconds(1.0);
		using encode_bitrate = bitrate_t<std::uint64_t, bits_per_second>;

		vk::VideoEncodeRateControlModeFlagBitsKHR m_rate_control_mode = vk::VideoEncodeRateControlModeFlagBitsKHR::eDefault;
		encode_bitrate m_average_bitrate{};
		encode_bitrate m_peak_bitrate{};
		bitrate m_bitrate_ceiling = bits_per_second(0.f);
		std::uint32_t m_frame_rate_numerator = 60;
		std::uint32_t m_frame_rate_denominator = 1;
		std::uint32_t m_quality_level = 0;
		std::int32_t m_constant_quantizer = 0;
		bool m_rate_control_dirty = true;
		device* m_device = nullptr;
		queue* m_queue = nullptr;
		bool m_direct_plane_writes = false;
	};
}

namespace gse::vulkan {
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
		gpu::video_codec codec
	) -> void;

	constexpr vk::DeviceSize bitstream_buffer_size = 4 * 1024 * 1024;
	constexpr std::uint32_t virtual_buffer_ms = 2000;
	constexpr std::uint32_t initial_virtual_buffer_ms = 1000;
	constexpr float peak_bitrate_headroom = 1.5f;
	constexpr std::uint32_t frame_rate_denominator = 1000;
	constexpr std::int32_t fallback_h265_qp = 24;
	constexpr std::int32_t fallback_av1_q_index = 100;
	constexpr auto nv12_format = vk::Format::eG8B8R82Plane420Unorm;
	constexpr auto y_plane_format = vk::Format::eR8Unorm;
	constexpr auto uv_plane_format = vk::Format::eR8G8Unorm;

	constexpr vk::ImageSubresourceRange color_subresource_range{
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	auto create_nv12_image(
		const vk::raii::Device& device,
		const physical_device& physical_device,
		vec2u extent,
		vk::ImageUsageFlags usage,
		const vk::VideoProfileListInfoKHR& profile_list,
		std::span<const std::uint32_t> shared_families = {}
	) -> std::tuple<vk::Image, vk::raii::ImageView, vk::DeviceMemory>;

	auto plane_writes_supported(
		const physical_device& physical_device,
		const vk::VideoProfileListInfoKHR& profile_list
	) -> bool;

	auto find_memory_type(
		const physical_device& physical_device,
		std::uint32_t type_bits,
		vk::MemoryPropertyFlags properties
	) -> std::uint32_t;

	auto select_rate_control(
		vk::VideoEncodeRateControlModeFlagsKHR modes
	) -> gpu::encode_rate_control;

	auto rate_control_mode_bit(
		gpu::encode_rate_control mode
	) -> vk::VideoEncodeRateControlModeFlagBitsKHR;
}

auto gse::vulkan::video_encoder::probe(device& dev, queue& q) -> gpu::encode_capabilities {
	if (!q.has_video_encode()) {
		return {};
	}

	const auto& physical = dev.physical_device();

	for (const auto codec : { gpu::video_codec::av1, gpu::video_codec::h265 }) {
		profile_chain chain{};
		build_profile(chain, codec);

		vk::VideoCapabilitiesKHR caps;
		vk::VideoEncodeCapabilitiesKHR encode_caps{};
		std::int32_t min_quantizer = 0;
		std::int32_t max_quantizer = 0;
		if (codec == gpu::video_codec::av1) {
			auto [caps_result, caps_chain] = std::bit_cast<vk::PhysicalDevice>(physical.handle())
				.getVideoCapabilitiesKHR<vk::VideoCapabilitiesKHR, vk::VideoEncodeCapabilitiesKHR, vk::VideoEncodeAV1CapabilitiesKHR>(chain.profile);
			if (caps_result != vk::Result::eSuccess) {
				continue;
			}
			caps = caps_chain.get<vk::VideoCapabilitiesKHR>();
			encode_caps = caps_chain.get<vk::VideoEncodeCapabilitiesKHR>();
			const auto& av1_caps = caps_chain.get<vk::VideoEncodeAV1CapabilitiesKHR>();
			min_quantizer = static_cast<std::int32_t>(av1_caps.minQIndex);
			max_quantizer = static_cast<std::int32_t>(av1_caps.maxQIndex);
		}
		else {
			auto [caps_result, caps_chain] = std::bit_cast<vk::PhysicalDevice>(physical.handle())
				.getVideoCapabilitiesKHR<vk::VideoCapabilitiesKHR, vk::VideoEncodeCapabilitiesKHR, vk::VideoEncodeH265CapabilitiesKHR>(chain.profile);
			if (caps_result != vk::Result::eSuccess) {
				continue;
			}
			caps = caps_chain.get<vk::VideoCapabilitiesKHR>();
			encode_caps = caps_chain.get<vk::VideoEncodeCapabilitiesKHR>();
			const auto& h265_caps = caps_chain.get<vk::VideoEncodeH265CapabilitiesKHR>();
			min_quantizer = h265_caps.minQp;
			max_quantizer = h265_caps.maxQp;
		}

		const auto rate_control = select_rate_control(encode_caps.rateControlModes);
		const bitrate max_bitrate = bits_per_second(encode_caps.maxBitrate);

		const auto codec_name = codec == gpu::video_codec::av1 ? "AV1" : "H.265";
		log::println(
			log::category::vulkan,
			"Video encode probe: {} supported (max {}x{}, rate control {}, max {:.1f:Mb/s}, {} quality levels)",
			codec_name,
			caps.maxCodedExtent.width,
			caps.maxCodedExtent.height,
			vk::to_string(rate_control_mode_bit(rate_control)),
			max_bitrate,
			encode_caps.maxQualityLevels
		);

		if (rate_control == gpu::encode_rate_control::driver_default) {
			log::println(
				log::level::warning,
				log::category::vulkan,
				"Video encode probe: {} exposes no explicit rate control mode; bitrate settings will not apply",
				codec_name
			);
		}

		return {
			.available = true,
			.codec = codec,
			.max_extent = { caps.maxCodedExtent.width, caps.maxCodedExtent.height },
			.std_header_name = caps.stdHeaderVersion.extensionName.data(),
			.std_header_spec_version = caps.stdHeaderVersion.specVersion,
			.rate_control = rate_control,
			.max_bitrate = max_bitrate,
			.quality_levels = encode_caps.maxQualityLevels,
			.min_quantizer = min_quantizer,
			.max_quantizer = max_quantizer
		};
	}

	log::println(log::category::vulkan, "Video encode probe: no supported codec found");
	return {};
}

auto gse::vulkan::video_encoder::create(device& dev, queue& q, const gpu::encode_desc& desc, const gpu::encode_capabilities& probe_caps) -> video_encoder {
	const auto extent = desc.extent;

	video_encoder enc;
	enc.m_device = &dev;
	enc.m_queue = &q;
	enc.m_codec = probe_caps.codec;
	enc.m_extent = extent;

	enc.m_rate_control_mode = rate_control_mode_bit(probe_caps.rate_control);
	enc.m_bitrate_ceiling = probe_caps.max_bitrate;
	enc.m_quality_level = probe_caps.quality_levels > 0 ? probe_caps.quality_levels - 1 : 0;

	if (enc.m_rate_control_mode == vk::VideoEncodeRateControlModeFlagBitsKHR::eDisabled) {
		const auto fallback = probe_caps.codec == gpu::video_codec::av1 ? fallback_av1_q_index : fallback_h265_qp;
		enc.m_constant_quantizer = std::clamp(fallback, probe_caps.min_quantizer, probe_caps.max_quantizer);
	}

	const time minimum_frame_interval = milliseconds(1.f);
	const time reference_period = seconds(1.f);
	const auto frame_interval = std::max(desc.frame_interval, minimum_frame_interval);
	const float frames_per_reference = reference_period / frame_interval;
	enc.m_frame_rate_denominator = frame_rate_denominator;
	enc.m_frame_rate_numerator = static_cast<std::uint32_t>(std::lround(frames_per_reference * static_cast<float>(frame_rate_denominator)));

	enc.set_bitrate(desc.average_bitrate);

	profile_chain chain{};
	build_profile(chain, probe_caps.codec);
	const auto& vk_dev = dev.raii_device();
	const auto& physical = dev.physical_device();
	const auto encode_family = q.video_encode_family_index().value();

	const auto family_properties = std::bit_cast<vk::PhysicalDevice>(physical.handle()).getQueueFamilyProperties();
	const auto valid_bits = family_properties[encode_family].timestampValidBits;
	enc.m_timestamp_ticks_mask = valid_bits >= 64 ? ~std::uint64_t{ 0 } : (std::uint64_t{ 1 } << valid_bits) - 1;
	enc.m_timestamp_period_per_tick = nanoseconds(static_cast<double>(physical.timestamp_period()));
	if (valid_bits == 0) {
		log::println(
			log::level::warning,
			log::category::vulkan,
			"Video encode queue family {} exposes no timestamp bits; encode work will not appear in the GPU profile",
			encode_family
		);
	}

	vk::VideoProfileListInfoKHR profile_list{
		.profileCount = 1,
		.pProfiles = &chain.profile
	};

	std::vector<std::uint32_t> shared_families{ q.graphics_family_index(), q.compute_family_index(), encode_family };
	std::ranges::sort(shared_families);
	shared_families.erase(std::ranges::unique(shared_families).begin(), shared_families.end());

	enc.m_direct_plane_writes = plane_writes_supported(physical, profile_list);
	if (!enc.m_direct_plane_writes) {
		log::println(
			log::level::warning,
			log::category::vulkan,
			"video_encoder: storage writes to {} are unsupported, falling back to per-frame plane copies on the encode queue",
			vk::to_string(nv12_format)
		);
	}

	vk::ExtensionProperties std_header_version{};
	const auto name_len = std::min(probe_caps.std_header_name.size(), std_header_version.extensionName.size() - 1);
	std::ranges::copy_n(
		probe_caps.std_header_name.begin(),
		static_cast<std::ptrdiff_t>(name_len),
		std_header_version.extensionName.begin()
	);
	std_header_version.specVersion = probe_caps.std_header_spec_version;
	auto [session_result, session] = vk_dev.createVideoSessionKHR({
		.queueFamilyIndex = encode_family,
		.pVideoProfile = &chain.profile,
		.pictureFormat = nv12_format,
		.maxCodedExtent = { extent.x(), extent.y() },
		.referencePictureFormat = nv12_format,
		.maxDpbSlots = 2,
		.maxActiveReferencePictures = 1,
		.pStdHeaderVersion = &std_header_version
	});
	assert(session_result == vk::Result::eSuccess, "failed to create video session: {}", vk::to_string(session_result));
	enc.m_session = std::move(session);

	auto [reqs_result, reqs] = enc.m_session.getMemoryRequirements();
	assert(reqs_result == vk::Result::eSuccess, "failed to query video session memory requirements: {}", vk::to_string(reqs_result));
	for (const auto& req : reqs) {
		const auto mem_type =
			find_memory_type(
				physical,
				req.memoryRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);
		auto [mem_result, mem] = (*vk_dev).allocateMemory({
			.allocationSize = req.memoryRequirements.size,
			.memoryTypeIndex = mem_type
		});
		assert(mem_result == vk::Result::eSuccess, "failed to allocate video session memory: {}", vk::to_string(mem_result));

		enc.m_session.bindMemory(
			vk::BindVideoSessionMemoryInfoKHR{
				.memoryBindIndex = req.memoryBindIndex,
				.memory = mem,
				.memoryOffset = 0,
				.memorySize = req.memoryRequirements.size
			}
		);
		enc.m_session_memory.push_back(mem);
	}

	if (probe_caps.codec == gpu::video_codec::h265) {
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

		const vk::VideoEncodeQualityLevelInfoKHR quality_info{
			.pNext = &info,
			.qualityLevel = enc.m_quality_level
		};

		auto [params_result, params] = vk_dev.createVideoSessionParametersKHR({
			.pNext = &quality_info,
			.videoSession = *enc.m_session
		});
		assert(params_result == vk::Result::eSuccess, "failed to create video session parameters: {}", vk::to_string(params_result));
		enc.m_params = std::move(params);
	}
	else {
		const auto bit_width_minus_1 = [](const std::uint32_t value) -> std::uint8_t {
			return static_cast<std::uint8_t>(std::bit_width(value) - 1);
		};

		static vk::video::AV1ColorConfig color_config{
			.flags = {
				.color_range = 1,
			},
			.BitDepth = 8,
			.subsampling_x = 1,
			.subsampling_y = 1,
			.color_primaries = vk::video::AV1ColorPrimaries::eBt709,
			.transfer_characteristics = vk::video::AV1TransferCharacteristics::eBt709,
			.matrix_coefficients = vk::video::AV1MatrixCoefficients::eBt709,
			.chroma_sample_position = vk::video::AV1ChromaSamplePosition::eUnknown,
		};

		static vk::video::AV1SequenceHeader seq_header{
			.flags = {
				.enable_order_hint = 1,
				.enable_cdef = 1,
				.enable_restoration = 1,
			},
			.seq_profile = vk::video::AV1Profile::eMain,
			.frame_width_bits_minus_1 = bit_width_minus_1(extent.x() - 1),
			.frame_height_bits_minus_1 = bit_width_minus_1(extent.y() - 1),
			.max_frame_width_minus_1 = static_cast<std::uint16_t>(extent.x() - 1),
			.max_frame_height_minus_1 = static_cast<std::uint16_t>(extent.y() - 1),
			.order_hint_bits_minus_1 = 7,
			.pColorConfig = &color_config,
		};

		auto info = vk::VideoEncodeAV1SessionParametersCreateInfoKHR{
			.pStdSequenceHeader = seq_header
		};

		const vk::VideoEncodeQualityLevelInfoKHR quality_info{
			.pNext = &info,
			.qualityLevel = enc.m_quality_level
		};

		auto [params_result, params] = vk_dev.createVideoSessionParametersKHR({
			.pNext = &quality_info,
			.videoSession = *enc.m_session
		});
		assert(params_result == vk::Result::eSuccess, "failed to create video session parameters: {}", vk::to_string(params_result));
		enc.m_params = std::move(params);
	}

	{
		const vk::VideoEncodeH265SessionParametersGetInfoKHR h265_get_info{
			.writeStdVPS = vk::True,
			.writeStdSPS = vk::True,
			.writeStdPPS = vk::True
		};
		const vk::VideoEncodeSessionParametersGetInfoKHR get_info{
			.pNext = probe_caps.codec == gpu::video_codec::h265 ? static_cast<const void*>(&h265_get_info) : nullptr,
			.videoSessionParameters = *enc.m_params
		};

		std::size_t data_size = 0;
		(void)(*vk_dev).getEncodedVideoSessionParametersKHR(&get_info, nullptr, &data_size, nullptr);
		enc.m_stream_header.resize(data_size);
		(void)(*vk_dev).getEncodedVideoSessionParametersKHR(&get_info, nullptr, &data_size, enc.m_stream_header.data());

		log::println(
			log::category::vulkan,
			"video_encoder: extracted {} bytes of session-parameter bitstream",
			enc.m_stream_header.size()
		);
	}

	for (auto& entry : enc.m_dpb) {
		auto [img, v, mem] =
			create_nv12_image(
				vk_dev,
				physical,
				extent,
				vk::ImageUsageFlagBits::eVideoEncodeDpbKHR,
				profile_list
			);
		entry.image = img;
		entry.view = std::move(v);
		entry.memory = mem;
	}

	const auto feedback_flags = vk::VideoEncodeFeedbackFlagBitsKHR::eBitstreamBufferOffset |
		vk::VideoEncodeFeedbackFlagBitsKHR::eBitstreamBytesWritten;
	vk::QueryPoolVideoEncodeFeedbackCreateInfoKHR feedback_info{
		.pNext = &chain.profile,
		.encodeFeedbackFlags = feedback_flags
	};

	for (auto& slot : enc.m_slots) {
		auto [pool_result, pool] = vk_dev.createCommandPool({
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = encode_family
		});
		assert(pool_result == vk::Result::eSuccess, "failed to create video command pool: {}", vk::to_string(pool_result));
		slot.pool = std::move(pool);

		auto [bufs_result, bufs] = vk_dev.allocateCommandBuffers({
			.commandPool = *slot.pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		});
		assert(bufs_result == vk::Result::eSuccess, "failed to allocate video command buffer: {}", vk::to_string(bufs_result));
		slot.cmd = std::move(bufs[0]);

		auto [fence_result, fence] = vk_dev.createFence({});
		assert(fence_result == vk::Result::eSuccess, "failed to create video fence: {}", vk::to_string(fence_result));
		slot.fence = std::move(fence);

		auto [query_pool_result, query_pool] = vk_dev.createQueryPool({
			.pNext = &feedback_info,
			.queryType = vk::QueryType::eVideoEncodeFeedbackKHR,
			.queryCount = 1
		});
		assert(query_pool_result == vk::Result::eSuccess, "failed to create video query pool: {}", vk::to_string(query_pool_result));
		slot.query_pool = std::move(query_pool);

		if (enc.timestamps_supported()) {
			auto [timestamp_pool_result, timestamp_pool] = vk_dev.createQueryPool({
				.queryType = vk::QueryType::eTimestamp,
				.queryCount = 2
			});
			assert(timestamp_pool_result == vk::Result::eSuccess, "failed to create video timestamp query pool: {}", vk::to_string(timestamp_pool_result));
			slot.timestamp_pool = std::move(timestamp_pool);
		}

		slot.bitstream = dev.create_buffer(
			gpu::buffer_desc{
				.size = bitstream_buffer_size,
				.usage = gpu::buffer_flag::video_encode_dst,
				.pnext = &profile_list
			},
			"encode_bitstream"
		);

		auto [img, v, mem] = create_nv12_image(
			vk_dev,
			physical,
			extent,
			enc.m_direct_plane_writes
				? vk::ImageUsageFlagBits::eVideoEncodeSrcKHR | vk::ImageUsageFlagBits::eStorage
				: vk::ImageUsageFlagBits::eVideoEncodeSrcKHR | vk::ImageUsageFlagBits::eTransferDst,
			profile_list,
			enc.m_direct_plane_writes ? std::span<const std::uint32_t>(shared_families) : std::span<const std::uint32_t>{}
		);
		slot.nv12_image = img;
		slot.nv12_view = std::move(v);
		slot.nv12_memory = mem;

		if (enc.m_direct_plane_writes) {
			const gpu::image y_plane(
				std::bit_cast<gpu::handle<gpu::image>>(img),
				{},
				gpu::image_format::r8_unorm,
				vec3u{ extent.x(), extent.y(), 1 },
				gpu::image_view_create_info{
					.format = gpu::image_format::r8_unorm,
					.view_type = gpu::image_view_type::e2d,
					.aspects = gpu::image_aspect_flag::plane_0,
				}
			);
			const gpu::image uv_plane(
				std::bit_cast<gpu::handle<gpu::image>>(img),
				{},
				gpu::image_format::r8g8_unorm,
				vec3u{ extent.x() / 2, extent.y() / 2, 1 },
				gpu::image_view_create_info{
					.format = gpu::image_format::r8g8_unorm,
					.view_type = gpu::image_view_type::e2d,
					.aspects = gpu::image_aspect_flag::plane_1,
				}
			);
			slot.y_plane_slot = dev.register_storage_image(y_plane);
			slot.uv_plane_slot = dev.register_storage_image(uv_plane);
		}
		else {
			slot.y_staging = dev.create_image(
				gpu::image_desc{
					.size = extent,
					.format = gpu::image_format::r8_unorm,
					.usage = { gpu::image_flag::storage, gpu::image_flag::transfer_src },
					.bindless = true
				},
				"encode.y_plane"
			);
			slot.uv_staging = dev.create_image(
				gpu::image_desc{
					.size = vec2u{ extent.x() / 2, extent.y() / 2 },
					.format = gpu::image_format::r8g8_unorm,
					.usage = { gpu::image_flag::storage, gpu::image_flag::transfer_src },
					.bindless = true
				},
				"encode.uv_plane"
			);
		}
	}

	enc.prime_source_layouts();

	const auto* const codec_name = probe_caps.codec == gpu::video_codec::av1 ? "AV1" : "H.265";
	log::println(
		log::category::vulkan,
		"Video encoder created: {} {}x{}, {} at {:.1f:Mb/s} (peak {:.1f:Mb/s}), {:.2f:ms} frame interval, quality level {}",
		codec_name,
		extent.x(),
		extent.y(),
		vk::to_string(enc.m_rate_control_mode),
		bitrate(enc.m_average_bitrate),
		bitrate(enc.m_peak_bitrate),
		frame_interval,
		enc.m_quality_level
	);

	return enc;
}

auto gse::vulkan::video_encoder::set_bitrate(const bitrate rate) -> void {
	const bitrate no_ceiling = bits_per_second(0.f);
	const bitrate minimum_bitrate = megabits_per_second(0.5f);

	const auto requested = std::max(rate, minimum_bitrate);
	const auto ceiling = m_bitrate_ceiling > no_ceiling ? m_bitrate_ceiling : requested;
	const auto average = std::min(requested, ceiling);
	const auto peak = m_rate_control_mode == vk::VideoEncodeRateControlModeFlagBitsKHR::eCbr
		? average
		: std::min(average * peak_bitrate_headroom, ceiling);

	m_average_bitrate = encode_bitrate(average);
	m_peak_bitrate = encode_bitrate(peak);
	m_rate_control_dirty = true;
}

auto gse::vulkan::video_encoder::record_rate_control(per_frame& slot, const bool reset) -> void {
	const bool metered = m_rate_control_mode == vk::VideoEncodeRateControlModeFlagBitsKHR::eCbr ||
		m_rate_control_mode == vk::VideoEncodeRateControlModeFlagBitsKHR::eVbr;

	const vk::VideoEncodeH265RateControlLayerInfoKHR h265_layer{};
	const vk::VideoEncodeAV1RateControlLayerInfoKHR av1_layer{};

	const vk::VideoEncodeRateControlLayerInfoKHR layer{
		.pNext = m_codec == gpu::video_codec::av1 ? static_cast<const void*>(&av1_layer) : &h265_layer,
		.averageBitrate = static_cast<std::uint64_t>(m_average_bitrate),
		.maxBitrate = static_cast<std::uint64_t>(m_peak_bitrate),
		.frameRateNumerator = m_frame_rate_numerator,
		.frameRateDenominator = m_frame_rate_denominator
	};

	const vk::VideoEncodeH265RateControlInfoKHR h265_rate_control{
		.gopFrameCount = m_gop_size,
		.idrPeriod = m_gop_size,
		.consecutiveBFrameCount = 0,
		.subLayerCount = 1
	};

	const vk::VideoEncodeAV1RateControlInfoKHR av1_rate_control{
		.gopFrameCount = m_gop_size,
		.keyFramePeriod = m_gop_size,
		.consecutiveBipredictiveFrameCount = 0,
		.temporalLayerCount = 1
	};

	const vk::VideoEncodeRateControlInfoKHR rate_control{
		.pNext = !metered ? nullptr : (m_codec == gpu::video_codec::av1 ? static_cast<const void*>(&av1_rate_control) : &h265_rate_control),
		.rateControlMode = m_rate_control_mode,
		.layerCount = metered ? 1u : 0u,
		.pLayers = metered ? &layer : nullptr,
		.virtualBufferSizeInMs = metered ? virtual_buffer_ms : 0u,
		.initialVirtualBufferSizeInMs = metered ? initial_virtual_buffer_ms : 0u
	};

	const vk::VideoEncodeQualityLevelInfoKHR quality{
		.pNext = &rate_control,
		.qualityLevel = m_quality_level
	};

	auto flags = vk::VideoCodingControlFlagBitsKHR::eEncodeRateControl | vk::VideoCodingControlFlagBitsKHR::eEncodeQualityLevel;
	if (reset) {
		flags |= vk::VideoCodingControlFlagBitsKHR::eReset;
	}

	slot.cmd.controlVideoCodingKHR({
		.pNext = &quality,
		.flags = flags
	});

	m_rate_control_dirty = false;
}

auto gse::vulkan::video_encoder::prime_source_layouts() -> void {
	const auto& vk_dev = m_device->raii_device();
	auto& primer = m_slots[0];

	primer.cmd.reset();
	primer.cmd.begin({
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	});

	std::vector<vk::ImageMemoryBarrier2> barriers;
	barriers.reserve(m_slots.size() * 3);
	for (const auto& slot : m_slots) {
		barriers.push_back({
			.srcStageMask = vk::PipelineStageFlagBits2::eNone,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eGeneral,
			.image = slot.nv12_image,
			.subresourceRange = color_subresource_range
		});
		if (m_direct_plane_writes) {
			continue;
		}
		barriers.push_back({
			.srcStageMask = vk::PipelineStageFlagBits2::eNone,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eGeneral,
			.image = std::bit_cast<vk::Image>(slot.y_staging.handle()),
			.subresourceRange = color_subresource_range
		});
		barriers.push_back({
			.srcStageMask = vk::PipelineStageFlagBits2::eNone,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eGeneral,
			.image = std::bit_cast<vk::Image>(slot.uv_staging.handle()),
			.subresourceRange = color_subresource_range
		});
	}

	primer.cmd.pipelineBarrier2({
		.imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
		.pImageMemoryBarriers = barriers.data()
	});
	primer.cmd.end();

	const gpu::command_buffer_submit_info cmd_submit{
		.command_buffer = std::bit_cast<gpu::command_buffer_handle>(*primer.cmd),
	};
	const gpu::submit_info submit{
		.command_buffers = std::span(&cmd_submit, 1),
	};
	m_queue->submit_video_encode(submit, std::bit_cast<gpu::handle<gpu::fence>>(*primer.fence));

	if (vk_dev.waitForFences(*primer.fence, vk::True, std::numeric_limits<std::uint64_t>::max()) != vk::Result::eSuccess) {
		log::println(log::level::warning, log::category::vulkan, "video_encoder: source layout priming timed out");
	}
	vk_dev.resetFences(*primer.fence);
}

auto gse::vulkan::video_encoder::begin_capture(const time pts) -> gpu::encode_source {
	auto& slot = m_slots[m_capture_number % source_ring_size];
	const auto& vk_dev = m_device->raii_device();

	if (slot.submitted) {
		constexpr std::uint64_t timeout_ns = 500'000'000;
		if (vk_dev.waitForFences(*slot.fence, vk::True, timeout_ns) != vk::Result::eSuccess) {
			log::println(
				log::level::warning,
				log::category::vulkan,
				"Video encode fence wait timed out; dropping capture {}",
				m_capture_number
			);
			slot.has_output = false;
			slot.submitted = false;
			slot.timestamps_pending = false;
			return {};
		}
		publish_encode_timestamps(slot);
		vk_dev.resetFences(*slot.fence);
		slot.submitted = false;
	}

	slot.capture_pts = pts;
	slot.captured = true;
	m_capture_number++;

	return {
		.y = m_direct_plane_writes ? slot.y_plane_slot.slot() : slot.y_staging.storage_slot(),
		.uv = m_direct_plane_writes ? slot.uv_plane_slot.slot() : slot.uv_staging.storage_slot(),
		.valid = true
	};
}

auto gse::vulkan::video_encoder::take_bitstream() -> std::optional<gpu::encoded_unit> {
	if (m_capture_number == 0) {
		return std::nullopt;
	}
	return read_slot_bitstream(m_slots[(m_capture_number - 1) % source_ring_size]);
}

auto gse::vulkan::video_encoder::submit_ready() -> void {
	if (m_capture_number <= encode_source_lag) {
		return;
	}

	auto& slot = m_slots[(m_capture_number - 1 - encode_source_lag) % source_ring_size];
	if (!slot.captured || slot.submitted) {
		return;
	}
	slot.captured = false;
	encode_capture(slot);
}

auto gse::vulkan::video_encoder::encode_capture(per_frame& slot) -> void {
	const auto extent = vec2u{ m_extent };
	const auto pts = slot.capture_pts;

	slot.cmd.reset();
	slot.cmd.begin({
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	});

	if (timestamps_supported()) {
		slot.cmd.resetQueryPool(*slot.timestamp_pool, 0, 2);
		slot.cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, *slot.timestamp_pool, 0);
	}

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

	if (m_direct_plane_writes) {
		const vk::ImageMemoryBarrier2 to_encode_src{
			.srcStageMask = vk::PipelineStageFlagBits2::eNone,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eVideoEncodeKHR,
			.dstAccessMask = vk::AccessFlagBits2::eVideoEncodeReadKHR,
			.oldLayout = vk::ImageLayout::eGeneral,
			.newLayout = vk::ImageLayout::eVideoEncodeSrcKHR,
			.image = slot.nv12_image,
			.subresourceRange = color_subresource_range
		};
		slot.cmd.pipelineBarrier2({
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &to_encode_src
		});
	}
	else {
		const auto y_source = std::bit_cast<vk::Image>(slot.y_staging.handle());
		const auto uv_source = std::bit_cast<vk::Image>(slot.uv_staging.handle());

		const std::array pre_barriers = { vk::ImageMemoryBarrier2{
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
				.srcStageMask = vk::PipelineStageFlagBits2::eNone,
				.srcAccessMask = vk::AccessFlagBits2::eNone,
				.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eTransferSrcOptimal,
				.image = y_source,
				.subresourceRange = color_subresource_range
										  },
			vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eNone,
				.srcAccessMask = vk::AccessFlagBits2::eNone,
				.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eTransferSrcOptimal,
				.image = uv_source,
				.subresourceRange = color_subresource_range
										  } };
		slot.cmd.pipelineBarrier2({
			.imageMemoryBarrierCount = static_cast<std::uint32_t>(pre_barriers.size()),
			.pImageMemoryBarriers = pre_barriers.data()
		});

		slot.cmd.copyImage(
			y_source,
			vk::ImageLayout::eTransferSrcOptimal,
			slot.nv12_image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageCopy{
				.srcSubresource = src_subresource,
				.dstSubresource = y_subresource,
				.extent = { extent.x(), extent.y(), 1 }
			}
		);

		slot.cmd.copyImage(
			uv_source,
			vk::ImageLayout::eTransferSrcOptimal,
			slot.nv12_image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageCopy{
				.srcSubresource = src_subresource,
				.dstSubresource = uv_subresource,
				.extent = { extent.x() / 2, extent.y() / 2, 1 }
			}
		);

		const std::array post_copy_barriers = { vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eVideoEncodeKHR,
			.dstAccessMask = vk::AccessFlagBits2::eVideoEncodeReadKHR,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eVideoEncodeSrcKHR,
			.image = slot.nv12_image,
			.subresourceRange = color_subresource_range
												},
			vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
				.dstStageMask = vk::PipelineStageFlagBits2::eNone,
				.dstAccessMask = vk::AccessFlagBits2::eNone,
				.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
				.newLayout = vk::ImageLayout::eGeneral,
				.image = y_source,
				.subresourceRange = color_subresource_range
												},
			vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
				.dstStageMask = vk::PipelineStageFlagBits2::eNone,
				.dstAccessMask = vk::AccessFlagBits2::eNone,
				.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
				.newLayout = vk::ImageLayout::eGeneral,
				.image = uv_source,
				.subresourceRange = color_subresource_range
												} };
		slot.cmd.pipelineBarrier2({
			.imageMemoryBarrierCount = static_cast<std::uint32_t>(post_copy_barriers.size()),
			.pImageMemoryBarriers = post_copy_barriers.data()
		});
	}

	const bool is_keyframe = (m_frame_number % m_gop_size) == 0;
	const auto dpb_index = static_cast<std::uint32_t>(m_frame_number % 2);
	const auto ref_index = dpb_index ^ 1u;
	auto& target_dpb = m_dpb[dpb_index];
	auto& ref_dpb = m_dpb[ref_index];
	const bool use_reference = !is_keyframe && ref_dpb.active;

	vk::VideoPictureResourceInfoKHR src_picture{
		.codedExtent = { extent.x(), extent.y() },
		.baseArrayLayer = 0,
		.imageViewBinding = *slot.nv12_view
	};

	vk::VideoPictureResourceInfoKHR dpb_picture{
		.codedExtent = { extent.x(), extent.y() },
		.baseArrayLayer = 0,
		.imageViewBinding = *target_dpb.view
	};

	vk::VideoPictureResourceInfoKHR ref_picture{
		.codedExtent = { extent.x(), extent.y() },
		.baseArrayLayer = 0,
		.imageViewBinding = *ref_dpb.view
	};

	const vk::video::EncodeAV1ReferenceInfo setup_av1_std{
		.frame_type = is_keyframe ? vk::video::AV1FrameType::eKey : vk::video::AV1FrameType::eInter,
		.OrderHint = static_cast<std::uint8_t>(m_frame_number & 0xFF)
	};
	const vk::VideoEncodeAV1DpbSlotInfoKHR setup_av1_dpb{
		.pStdReferenceInfo = setup_av1_std
	};
	const vk::video::EncodeAV1ReferenceInfo ref_av1_std{
		.frame_type = ref_dpb.av1_frame_type,
		.OrderHint = ref_dpb.av1_order_hint
	};
	const vk::VideoEncodeAV1DpbSlotInfoKHR ref_av1_dpb{
		.pStdReferenceInfo = ref_av1_std
	};

	const vk::video::EncodeH265ReferenceInfo setup_h265_std{
		.pic_type = is_keyframe ? vk::video::H265PictureType::eIdr : vk::video::H265PictureType::eP,
		.PicOrderCntVal = static_cast<std::int32_t>(m_frame_number & 0xFF)
	};
	const vk::VideoEncodeH265DpbSlotInfoKHR setup_h265_dpb{
		.pStdReferenceInfo = setup_h265_std
	};
	const vk::video::EncodeH265ReferenceInfo ref_h265_std{
		.pic_type = ref_dpb.h265_pic_type,
		.PicOrderCntVal = ref_dpb.h265_poc
	};
	const vk::VideoEncodeH265DpbSlotInfoKHR ref_h265_dpb{
		.pStdReferenceInfo = ref_h265_std
	};

	const void* setup_dpb_pnext = m_codec == gpu::video_codec::av1 ? static_cast<const void*>(&setup_av1_dpb) : &setup_h265_dpb;
	const void* ref_dpb_pnext = !use_reference
		? nullptr
		: (m_codec == gpu::video_codec::av1 ? static_cast<const void*>(&ref_av1_dpb) : &ref_h265_dpb);

	vk::VideoReferenceSlotInfoKHR setup_ref{
		.pNext = setup_dpb_pnext,
		.slotIndex = static_cast<std::int32_t>(dpb_index),
		.pPictureResource = &dpb_picture
	};

	std::vector<vk::VideoReferenceSlotInfoKHR> begin_ref_slots;
	if (!is_keyframe) {
		if (use_reference) {
			begin_ref_slots.push_back({
				.pNext = ref_dpb_pnext,
				.slotIndex = static_cast<std::int32_t>(ref_index),
				.pPictureResource = &ref_picture
			});
		}
		if (target_dpb.active) {
			begin_ref_slots.push_back({
				.slotIndex = static_cast<std::int32_t>(dpb_index),
				.pPictureResource = nullptr
			});
		}
	}
	begin_ref_slots.push_back({
		.pNext = setup_dpb_pnext,
		.slotIndex = -1,
		.pPictureResource = &dpb_picture
	});

	slot.cmd.resetQueryPool(*slot.query_pool, 0, 1);

	const vk::MemoryBarrier2 encode_sync{
		.srcStageMask = vk::PipelineStageFlagBits2::eVideoEncodeKHR,
		.srcAccessMask = vk::AccessFlagBits2::eVideoEncodeWriteKHR,
		.dstStageMask = vk::PipelineStageFlagBits2::eVideoEncodeKHR,
		.dstAccessMask = vk::AccessFlagBits2::eVideoEncodeReadKHR | vk::AccessFlagBits2::eVideoEncodeWriteKHR
	};
	slot.cmd.pipelineBarrier2({
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &encode_sync
	});

	slot.cmd.beginVideoCodingKHR({
		.videoSession = *m_session,
		.videoSessionParameters = *m_params,
		.referenceSlotCount = static_cast<std::uint32_t>(begin_ref_slots.size()),
		.pReferenceSlots = begin_ref_slots.data()
	});

	if (is_keyframe || m_rate_control_dirty) {
		record_rate_control(slot, is_keyframe);
	}

	slot.cmd.beginQuery(
		*slot.query_pool,
		0,
		{}
	);

	if (m_codec == gpu::video_codec::h265) {
		vk::video::EncodeH265ReferenceListsInfo ref_lists{};
		for (auto& r : ref_lists.RefPicList0) {
			r = 0xFF;
		}
		for (auto& r : ref_lists.RefPicList1) {
			r = 0xFF;
		}
		if (use_reference) {
			ref_lists.RefPicList0[0] = static_cast<std::uint8_t>(ref_index);
		}

		const vk::video::H265ShortTermRefPicSet strps{
			.used_by_curr_pic_s0_flag = use_reference ? std::uint16_t{ 0x1 } : std::uint16_t{ 0 },
			.num_negative_pics = use_reference ? std::uint8_t{ 1 } : std::uint8_t{ 0 }
		};

		const vk::video::EncodeH265PictureInfo std_pic_info{
			.flags = {
				.is_reference = 1,
				.IrapPicFlag = is_keyframe ? 1u : 0u,
				.short_term_ref_pic_set_sps_flag = 0
			},
			.pic_type = is_keyframe ? vk::video::H265PictureType::eIdr : vk::video::H265PictureType::eP,
			.PicOrderCntVal = static_cast<std::int32_t>(m_frame_number & 0xFF),
			.pRefLists = &ref_lists,
			.pShortTermRefPicSet = &strps
		};

		const vk::video::EncodeH265SliceSegmentHeader slice_header{
			.flags = {
				.first_slice_segment_in_pic_flag = 1
			},
			.slice_type = is_keyframe ? vk::video::H265SliceType::eI : vk::video::H265SliceType::eP
		};

		const vk::VideoEncodeH265NaluSliceSegmentInfoKHR nalu{
			.constantQp = m_constant_quantizer,
			.pStdSliceSegmentHeader = slice_header
		};

		const vk::VideoEncodeH265PictureInfoKHR h265_pic{
			.naluSliceSegmentEntryCount = 1,
			.pNaluSliceSegmentEntries = &nalu,
			.pStdPictureInfo = std_pic_info
		};

		const vk::VideoReferenceSlotInfoKHR encode_ref_slot{
			.pNext = &ref_h265_dpb,
			.slotIndex = static_cast<std::int32_t>(ref_index),
			.pPictureResource = &ref_picture
		};

		slot.cmd.encodeVideoKHR({
			.pNext = &h265_pic,
			.dstBuffer = std::bit_cast<vk::Buffer>(slot.bitstream.handle()),
			.dstBufferOffset = 0,
			.dstBufferRange = bitstream_buffer_size,
			.srcPictureResource = src_picture,
			.pSetupReferenceSlot = &setup_ref,
			.referenceSlotCount = use_reference ? 1u : 0u,
			.pReferenceSlots = use_reference ? &encode_ref_slot : nullptr
		});
	}
	else {
		vk::video::EncodeAV1PictureInfo std_pic_info{
			.flags = {
				.show_frame = 1,
			},
			.frame_type = is_keyframe ? vk::video::AV1FrameType::eKey : vk::video::AV1FrameType::eInter,
			.order_hint = static_cast<std::uint8_t>(m_frame_number & 0xFF),
			.primary_ref_frame = use_reference ? std::uint8_t{ 0 } : std::uint8_t{ 7 },
			.refresh_frame_flags = 0xFF
		};
		for (auto& idx : std_pic_info.ref_frame_idx) {
			idx = -1;
		}

		vk::VideoEncodeAV1PictureInfoKHR av1_pic{
			.predictionMode = use_reference ? vk::VideoEncodeAV1PredictionModeKHR::eSingleReference
											: vk::VideoEncodeAV1PredictionModeKHR::eIntraOnly,
			.rateControlGroup = is_keyframe ? vk::VideoEncodeAV1RateControlGroupKHR::eIntra
											: vk::VideoEncodeAV1RateControlGroupKHR::ePredictive,
			.constantQIndex = static_cast<std::uint32_t>(m_constant_quantizer),
			.pStdPictureInfo = std_pic_info
		};
		for (auto& idx : av1_pic.referenceNameSlotIndices) {
			idx = -1;
		}
		if (use_reference) {
			av1_pic.referenceNameSlotIndices[0] = static_cast<std::int32_t>(ref_index);
		}

		const vk::VideoReferenceSlotInfoKHR encode_ref_slot{
			.pNext = &ref_av1_dpb,
			.slotIndex = static_cast<std::int32_t>(ref_index),
			.pPictureResource = &ref_picture
		};

		slot.cmd.encodeVideoKHR({
			.pNext = &av1_pic,
			.dstBuffer = std::bit_cast<vk::Buffer>(slot.bitstream.handle()),
			.dstBufferOffset = 0,
			.dstBufferRange = bitstream_buffer_size,
			.srcPictureResource = src_picture,
			.pSetupReferenceSlot = &setup_ref,
			.referenceSlotCount = use_reference ? 1u : 0u,
			.pReferenceSlots = use_reference ? &encode_ref_slot : nullptr
		});
	}

	slot.cmd.endQuery(*slot.query_pool, 0);

	slot.cmd.endVideoCodingKHR({});

	if (m_direct_plane_writes) {
		const vk::ImageMemoryBarrier2 to_general{
			.srcStageMask = vk::PipelineStageFlagBits2::eVideoEncodeKHR,
			.srcAccessMask = vk::AccessFlagBits2::eVideoEncodeReadKHR,
			.dstStageMask = vk::PipelineStageFlagBits2::eNone,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eVideoEncodeSrcKHR,
			.newLayout = vk::ImageLayout::eGeneral,
			.image = slot.nv12_image,
			.subresourceRange = color_subresource_range
		};
		slot.cmd.pipelineBarrier2({
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &to_general
		});
	}

	if (timestamps_supported()) {
		slot.cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, *slot.timestamp_pool, 1);
	}

	slot.cmd.end();

	slot.last_pts = pts;
	slot.last_was_keyframe = is_keyframe;
	slot.cpu_ref = system_clock::now<trace::tick_step>();
	slot.timestamp_frame = m_frame_number;

	const gpu::command_buffer_submit_info cmd_submit{
		.command_buffer = std::bit_cast<gpu::command_buffer_handle>(*slot.cmd),
	};
	const gpu::submit_info submit{
		.command_buffers = std::span(&cmd_submit, 1),
	};
	m_queue->submit_video_encode(submit, std::bit_cast<gpu::handle<gpu::fence>>(*slot.fence));
	slot.submitted = true;
	slot.has_output = true;
	slot.timestamps_pending = timestamps_supported();

	if (is_keyframe) {
		for (auto& dpb : m_dpb) {
			dpb.active = false;
		}
	}
	target_dpb.active = true;
	if (m_codec == gpu::video_codec::av1) {
		target_dpb.av1_frame_type = setup_av1_std.frame_type;
		target_dpb.av1_order_hint = setup_av1_std.OrderHint;
	}
	else {
		target_dpb.h265_pic_type = setup_h265_std.pic_type;
		target_dpb.h265_poc = setup_h265_std.PicOrderCntVal;
	}

	m_frame_number++;
}

auto gse::vulkan::video_encoder::read_slot_bitstream(per_frame& slot) -> std::optional<gpu::encoded_unit> {
	if (!slot.has_output) {
		return std::nullopt;
	}

	const auto& vk_dev = m_device->raii_device();

	struct feedback_result {
		std::uint32_t offset;
		std::uint32_t bytes_written;
	} feedback{};

	const auto result =
		(*vk_dev).getQueryPoolResults(
			*slot.query_pool,
			0,
			1,
			sizeof(feedback),
			&feedback,
			sizeof(feedback),
			{}
		);

	if (result != vk::Result::eSuccess) {
		if (result != vk::Result::eNotReady) {
			log::println(
				log::level::warning,
				log::category::vulkan,
				"Video encode feedback query failed: result={}",
				static_cast<int>(result)
			);
		}
		slot.has_output = false;
		return std::nullopt;
	}

	if (feedback.bytes_written == 0) {
		slot.has_output = false;
		return std::nullopt;
	}

	gpu::encoded_unit unit;
	unit.bytes.resize(feedback.bytes_written);
	unit.pts = slot.last_pts;
	unit.keyframe = slot.last_was_keyframe;

	const auto src = slot.bitstream.host_read().subspan(feedback.offset);
	memcpy(unit.bytes.data(), src.data(), feedback.bytes_written);

	slot.has_output = false;
	return unit;
}

auto gse::vulkan::video_encoder::stream_header() const -> std::span<const std::byte> {
	return m_stream_header;
}

auto gse::vulkan::video_encoder::codec() const -> gpu::video_codec {
	return m_codec;
}

auto gse::vulkan::video_encoder::extent() const -> vec2u {
	return m_extent;
}

auto gse::vulkan::video_encoder::timestamps_supported() const -> bool {
	return m_timestamp_ticks_mask != 0;
}

auto gse::vulkan::video_encoder::publish_encode_timestamps(per_frame& slot) -> void {
	if (!slot.timestamps_pending) {
		return;
	}
	slot.timestamps_pending = false;

	std::array<std::uint64_t, 2> ticks{};
	const auto result =
		(*m_device->raii_device()).getQueryPoolResults(
			*slot.timestamp_pool,
			0,
			2,
			sizeof(ticks),
			ticks.data(),
			sizeof(std::uint64_t),
			vk::QueryResultFlagBits::e64
		);

	if (result != vk::Result::eSuccess) {
		return;
	}

	const auto begin_ticks = ticks[0] & m_timestamp_ticks_mask;
	const auto end_ticks = ticks[1] & m_timestamp_ticks_mask;
	if (end_ticks < begin_ticks) {
		return;
	}

	const auto span = static_cast<double>(end_ticks - begin_ticks) * m_timestamp_period_per_tick;
	const auto begin = time_t<double>(slot.cpu_ref);
	const auto end = begin + span;

	const auto encode_id = trace_id<"video::encode">();
	const auto key = (slot.timestamp_frame << 16) | (static_cast<std::uint64_t>(gpu::queue_type::video_encode) << 14);

	trace::begin_async_at(encode_id, key, trace::gpu_video_encode_virtual_tid, time_t<std::uint64_t>(begin));
	trace::end_async_at(encode_id, key, trace::gpu_video_encode_virtual_tid, time_t<std::uint64_t>(end));

	profile::ingest_gpu_sample(encode_id, span);
}

auto gse::vulkan::video_encoder::valid() const -> bool {
	return m_session != nullptr;
}

template <typename To, typename From>
constexpr auto gse::vulkan::vk_enum(const From v) -> To {
	return static_cast<To>(static_cast<std::underlying_type_t<From>>(v));
}

auto gse::vulkan::build_profile(profile_chain& chain, const gpu::video_codec codec) -> void {
	chain = {};

	chain.usage = {
		.videoUsageHints = vk::VideoEncodeUsageFlagBitsKHR::eRecording,
		.videoContentHints = vk::VideoEncodeContentFlagBitsKHR::eRendered,
		.tuningMode = vk::VideoEncodeTuningModeKHR::eLowLatency
	};

	if (codec == gpu::video_codec::h265) {
		chain.h265_profile.stdProfileIdc =
			vk_enum<decltype(chain.h265_profile.stdProfileIdc)>(vk::video::H265ProfileIdc::eMain);
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

auto gse::vulkan::plane_writes_supported(const physical_device& physical_device, const vk::VideoProfileListInfoKHR& profile_list) -> bool {
	const std::array view_formats = { nv12_format, y_plane_format, uv_plane_format };
	const vk::ImageFormatListCreateInfo format_list{
		.pNext = &profile_list,
		.viewFormatCount = static_cast<std::uint32_t>(view_formats.size()),
		.pViewFormats = view_formats.data()
	};

	const vk::PhysicalDeviceImageFormatInfo2 info{
		.pNext = &format_list,
		.format = nv12_format,
		.type = vk::ImageType::e2D,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eVideoEncodeSrcKHR | vk::ImageUsageFlagBits::eStorage,
		.flags = vk::ImageCreateFlagBits::eMutableFormat | vk::ImageCreateFlagBits::eExtendedUsage
	};

	auto [result, props] = std::bit_cast<vk::PhysicalDevice>(physical_device.handle()).getImageFormatProperties2(info);
	return result == vk::Result::eSuccess && props.imageFormatProperties.maxExtent.width > 0;
}

auto gse::vulkan::create_nv12_image(const vk::raii::Device& device, const physical_device& physical_device, vec2u extent, vk::ImageUsageFlags usage, const vk::VideoProfileListInfoKHR& profile_list, const std::span<const std::uint32_t> shared_families) -> std::tuple<vk::Image, vk::raii::ImageView, vk::DeviceMemory> {
	const bool writable = static_cast<bool>(usage & vk::ImageUsageFlagBits::eStorage);
	const std::array view_formats = { nv12_format, y_plane_format, uv_plane_format };
	const vk::ImageFormatListCreateInfo format_list{
		.pNext = &profile_list,
		.viewFormatCount = static_cast<std::uint32_t>(view_formats.size()),
		.pViewFormats = view_formats.data()
	};

	const bool concurrent = shared_families.size() > 1;
	auto [image_result, image] = (*device).createImage({
		.pNext = writable ? static_cast<const void*>(&format_list) : static_cast<const void*>(&profile_list),
		.flags = writable
			? vk::ImageCreateFlags{ vk::ImageCreateFlagBits::eMutableFormat | vk::ImageCreateFlagBits::eExtendedUsage }
			: vk::ImageCreateFlags{},
		.imageType = vk::ImageType::e2D,
		.format = nv12_format,
		.extent = { extent.x(), extent.y(), 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = usage,
		.sharingMode = concurrent ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = concurrent ? static_cast<std::uint32_t>(shared_families.size()) : 0u,
		.pQueueFamilyIndices = concurrent ? shared_families.data() : nullptr
	});
	assert(image_result == vk::Result::eSuccess, "failed to create nv12 image: {}", vk::to_string(image_result));

	const auto mem_reqs = (*device).getImageMemoryRequirements(image);
	const auto mem_props = physical_device.memory_properties();

	std::uint32_t mem_type = 0;
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((mem_reqs.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)) {
			mem_type = i;
			break;
		}
	}

	auto [memory_result, memory] = (*device).allocateMemory({
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = mem_type
	});
	assert(memory_result == vk::Result::eSuccess, "failed to allocate nv12 memory: {}", vk::to_string(memory_result));

	const auto bind_result = (*device).bindImageMemory(image, memory, 0);
	assert(bind_result == vk::Result::eSuccess, "failed to bind nv12 image memory: {}", vk::to_string(bind_result));

	auto [view_result, view] = device.createImageView({
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = nv12_format,
		.subresourceRange = color_subresource_range
	});
	assert(view_result == vk::Result::eSuccess, "failed to create nv12 image view: {}", vk::to_string(view_result));

	return { image, std::move(view), memory };
}

auto gse::vulkan::find_memory_type(const physical_device& physical_device, std::uint32_t type_bits, vk::MemoryPropertyFlags properties) -> std::uint32_t {
	const auto mem_props = physical_device.memory_properties();
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if ((type_bits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if (type_bits & (1u << i)) {
			return i;
		}
	}
	log::println(
		log::level::error,
		log::category::vulkan,
		"find_memory_type: no memory type matches bits 0x{:x}",
		type_bits
	);
	return 0;
}

auto gse::vulkan::select_rate_control(const vk::VideoEncodeRateControlModeFlagsKHR modes) -> gpu::encode_rate_control {
	if (modes & vk::VideoEncodeRateControlModeFlagBitsKHR::eVbr) {
		return gpu::encode_rate_control::variable_bitrate;
	}
	if (modes & vk::VideoEncodeRateControlModeFlagBitsKHR::eCbr) {
		return gpu::encode_rate_control::constant_bitrate;
	}
	if (modes & vk::VideoEncodeRateControlModeFlagBitsKHR::eDisabled) {
		return gpu::encode_rate_control::disabled;
	}
	return gpu::encode_rate_control::driver_default;
}

auto gse::vulkan::rate_control_mode_bit(const gpu::encode_rate_control mode) -> vk::VideoEncodeRateControlModeFlagBitsKHR {
	switch (mode) {
		case gpu::encode_rate_control::variable_bitrate:
			return vk::VideoEncodeRateControlModeFlagBitsKHR::eVbr;
		case gpu::encode_rate_control::constant_bitrate:
			return vk::VideoEncodeRateControlModeFlagBitsKHR::eCbr;
		case gpu::encode_rate_control::disabled:
			return vk::VideoEncodeRateControlModeFlagBitsKHR::eDisabled;
		case gpu::encode_rate_control::driver_default:
			return vk::VideoEncodeRateControlModeFlagBitsKHR::eDefault;
	}
	return vk::VideoEncodeRateControlModeFlagBitsKHR::eDefault;
}
