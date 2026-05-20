export module gse.network:input_frame;

import std;

import :message;

import gse.math;
import gse.time;

export namespace gse::network {
	struct axes1_pair {
		std::uint16_t id;
		float value;
	};

	struct axes2_pair {
		std::uint16_t id;
		float x, y;
	};

	struct[[= network_message{}]] input_frame {
		std::uint32_t input_sequence = 0;
		time_t<std::uint32_t, milliseconds> client_time = {};
		float camera_yaw = 0.f;
		std::vector<std::uint64_t> pressed;
		std::vector<std::uint64_t> released;
		std::vector<std::uint64_t> held;
		std::vector<axes1_pair> axes1;
		std::vector<axes2_pair> axes2;
	};
}
