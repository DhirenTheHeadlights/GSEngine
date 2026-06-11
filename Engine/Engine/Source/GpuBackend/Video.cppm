export module gse.gpu_backend:video;

import std;

import gse.math;
import gse.time;

export namespace gse::gpu {
	enum class video_codec : std::uint8_t {
		av1,
		h265
	};

	struct encode_capabilities {
		bool available = false;
		video_codec codec = video_codec::av1;
		vec2u max_extent;
		std::string std_header_name;
		std::uint32_t std_header_spec_version = 0;
	};

	struct encoded_unit {
		std::vector<std::byte> bytes;
		time pts;
		bool keyframe = false;
	};
}
