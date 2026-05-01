export module gse.server:input_source;

import std;
import gse;

import :server;

export namespace gse {
	auto make_server_input_sampler(
		const std::unordered_map<network::address, client_data>* clients
	) -> input_sampler_fn;
}

auto gse::make_server_input_sampler(const std::unordered_map<network::address, client_data>* clients) -> input_sampler_fn {
	return [clients](world& w, std::function<void(const evaluation_context&)> fn) {
		if (!clients) {
			return;
		}

		auto* sc = w.current_scene();
		if (!sc) {
			return;
		}

		auto* reg = &sc->registry();

		for (const auto& cd : *clients | std::views::values) {
			auto* pc = reg->try_component<player_controller>(cd.controller_id);
			if (!pc) {
				continue;
			}

			const auto controlled_id = pc->controlled_entity_id;
			if (!controlled_id.exists()) {
				continue;
			}

			const actions::state& s = cd.latest_input;

			evaluation_context ctx{
				.client_id = controlled_id,
				.input = &s,
				.registry = reg,
			};

			actions::set_entity_camera_yaw(controlled_id, s.camera_yaw());
			actions::set_context_camera_yaw(s.camera_yaw());
			fn(ctx);
			actions::clear_context_camera_yaw();
		}
	};
}
