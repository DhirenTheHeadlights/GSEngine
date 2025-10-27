export module gse.network:actions;

import std;

import gse.platform;

export namespace gse::network::actions {
	struct snapshot {
        std::uint32_t client_tick{};
        std::int8_t move_x{};
        std::int8_t move_y{};
        std::int16_t look_x_mdeg{};
        std::int16_t look_y_mdeg{};

        std::vector<gse::actions::word> held;
        std::vector<gse::actions::word> pressed;
        std::vector<gse::actions::word> released;
	};
}