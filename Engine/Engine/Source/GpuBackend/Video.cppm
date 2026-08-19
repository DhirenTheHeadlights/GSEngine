export module gse.gpu_backend:video;

import std;

import :bindless;

import gse.math;
import gse.time;

export namespace gse::gpu {
	enum class video_codec : std::uint8_t {
		av1,
		h265
	};

	enum class encode_rate_control : std::uint8_t {
		driver_default,
		disabled,
		constant_bitrate,
		variable_bitrate
	};

	struct encode_capabilities {
		bool available = false;
		video_codec codec = video_codec::av1;
		vec2u max_extent;
		std::string std_header_name;
		std::uint32_t std_header_spec_version = 0;
		encode_rate_control rate_control = encode_rate_control::driver_default;
		bitrate max_bitrate = bits_per_second(0.f);
		std::uint32_t quality_levels = 1;
		std::int32_t min_quantizer = 0;
		std::int32_t max_quantizer = 0;
	};

	struct encode_desc {
		vec2u extent;
		bitrate average_bitrate = megabits_per_second(15.f);
		time frame_interval = seconds(1.f / 60.f);
	};

	struct encoded_unit {
		std::vector<std::byte> bytes;
		time pts;
		bool keyframe = false;
	};

	struct encode_source {
		bindless_slot y;
		bindless_slot uv;
		bool valid = false;
	};
}
