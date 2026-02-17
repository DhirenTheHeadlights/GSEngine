export module gse.runtime:actions_api;

import std;

import gse.utility;
import gse.math;
import gse.platform;

import :core_api;

export namespace gse::actions {
	template <fixed_string Tag>
	auto add(
		const key default_key
	) -> handle {
		const id action_id = generate_id(Tag.view());

		channel_add(add_action_request{
			.name = std::string(Tag.view()),
			.default_key = default_key,
			.action_id = action_id
		});

		return handle(action_id);
	}

	auto bind_axis2(
		const pending_axis2_info& info
	) -> id {
		auto hash_combine = [](std::size_t& seed, const id& v) {
			seed ^= std::hash<std::uint64_t>{}(v.number()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		};

		std::size_t h = 0;
		hash_combine(h, info.left.id());
		hash_combine(h, info.right.id());
		hash_combine(h, info.back.id());
		hash_combine(h, info.fwd.id());

		const id axis_id = generate_id(std::to_string(h));

		channel_add(bind_axis2_request{
			.info = info,
			.axis_id = axis_id
		});

		return axis_id;
	}

	template <fixed_string Tag>
	auto bind_button_channel(
		const id owner_id,
		const key default_key,
		button_channel& channel
	) -> void {
		auto h = add<Tag>(default_key);
		channel.action_id = h.id();
		state_of<system_state>().register_channel(owner_id, channel);
	}

	auto bind_axis2_channel(
		const id owner_id,
		const pending_axis2_info& info,
		axis2_channel& channel,
		const id axis_id
	) -> void {
		channel.axis_id = axis_id;
		state_of<system_state>().register_channel(owner_id, channel);

		channel_add(bind_axis2_request{
			.info = info,
			.axis_id = axis_id
		});
	}

	auto register_channel(
		const id owner_id,
		button_channel& channel
	) -> void {
		state_of<system_state>().register_channel(owner_id, channel);
	}

	auto register_channel(
		const id owner_id,
		axis1_channel& channel
	) -> void {
		state_of<system_state>().register_channel(owner_id, channel);
	}

	auto register_channel(
		const id owner_id,
		axis2_channel& channel
	) -> void {
		state_of<system_state>().register_channel(owner_id, channel);
	}

	auto sample_for_entity(
		const state& s,
		const id owner_id
	) -> void {
		state_of<system_state>().sample_for_entity(s, owner_id);
	}

	auto sample_all_channels(
		const state& s
	) -> void {
		state_of<system_state>().sample_all_channels(s);
	}

	auto current_state(
	) -> const state& {
		return state_of<system_state>().current_state();
	}

	auto axis1_ids(
	) -> std::span<const std::uint16_t> {
		return state_of<system_state>().axis1_ids();
	}

	auto axis2_ids(
	) -> std::span<const std::uint16_t> {
		return state_of<system_state>().axis2_ids();
	}

	auto held(
		const id action_id
	) -> bool {
		auto& sys = state_of<system_state>();
		if (const auto* desc = sys.description(action_id)) {
			return sys.current_state().held(desc->bit_index());
		}
		return false;
	}

	auto pressed(
		const id action_id
	) -> bool {
		auto& sys = state_of<system_state>();
		if (const auto* desc = sys.description(action_id)) {
			return sys.current_state().pressed(desc->bit_index());
		}
		return false;
	}

	auto released(
		const id action_id
	) -> bool {
		auto& sys = state_of<system_state>();
		if (const auto* desc = sys.description(action_id)) {
			return sys.current_state().released(desc->bit_index());
		}
		return false;
	}

	auto pressed_mask(
	) -> const mask& {
		return state_of<system_state>().current_state().pressed_mask();
	}

	auto released_mask(
	) -> const mask& {
		return state_of<system_state>().current_state().released_mask();
	}

	auto axis1(
		const id axis_id
	) -> float {
		return state_of<system_state>().current_state().axis1(static_cast<std::uint16_t>(axis_id.number()));
	}

	auto axis2(
		const id axis_id
	) -> axis {
		return state_of<system_state>().current_state().axis2_v(static_cast<std::uint16_t>(axis_id.number()));
	}

	auto held(
		const state& s,
		const id action_id
	) -> bool {
		if (const auto* desc = state_of<system_state>().description(action_id)) {
			return s.held(desc->bit_index());
		}
		return false;
	}

	auto pressed(
		const state& s,
		const id action_id
	) -> bool {
		if (const auto* desc = state_of<system_state>().description(action_id)) {
			return s.pressed(desc->bit_index());
		}
		return false;
	}

	auto released(
		const state& s,
		const id action_id
	) -> bool {
		if (const auto* desc = state_of<system_state>().description(action_id)) {
			return s.released(desc->bit_index());
		}
		return false;
	}

	auto held(
		const handle& h,
		const state& s
	) -> bool {
		return h.held(s, state_of<system_state>());
	}

	auto pressed(
		const handle& h,
		const state& s
	) -> bool {
		return h.pressed(s, state_of<system_state>());
	}

	auto released(
		const handle& h,
		const state& s
	) -> bool {
		return h.released(s, state_of<system_state>());
	}
}