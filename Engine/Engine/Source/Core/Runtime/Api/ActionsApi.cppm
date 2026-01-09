export module gse.runtime:actions_api;

import std;

import gse.utility;
import gse.physics.math;
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
		system_of<system>().register_channel(owner_id, channel);
	}

	auto bind_axis2_channel(
		const id owner_id,
		const pending_axis2_info& info,
		axis2_channel& channel,
		const id axis_id
	) -> void {
		channel.axis_id = axis_id;
		system_of<system>().register_channel(owner_id, channel);

		channel_add(bind_axis2_request{
			.info = info,
			.axis_id = axis_id
		});
	}

	auto register_channel(
		const id owner_id,
		button_channel& channel
	) -> void {
		system_of<system>().register_channel(owner_id, channel);
	}

	auto register_channel(
		const id owner_id,
		axis1_channel& channel
	) -> void {
		system_of<system>().register_channel(owner_id, channel);
	}

	auto register_channel(
		const id owner_id,
		axis2_channel& channel
	) -> void {
		system_of<system>().register_channel(owner_id, channel);
	}

	auto sample_for_entity(
		const state& s,
		const id owner_id
	) -> void {
		system_of<system>().sample_for_entity(s, owner_id);
	}

	auto sample_all_channels(
		const state& s
	) -> void {
		system_of<system>().sample_all_channels(s);
	}

	auto current_state(
	) -> const state& {
		return system_of<system>().current_state();
	}

	auto axis1_ids(
	) -> std::span<const std::uint16_t> {
		return system_of<system>().axis1_ids();
	}

	auto axis2_ids(
	) -> std::span<const std::uint16_t> {
		return system_of<system>().axis2_ids();
	}

	auto held(
		const id action_id
	) -> bool {
		auto& sys = system_of<system>();
		if (const auto* desc = sys.description(action_id)) {
			return sys.current_state().held(desc->bit_index());
		}
		return false;
	}

	auto pressed(
		const id action_id
	) -> bool {
		auto& sys = system_of<system>();
		if (const auto* desc = sys.description(action_id)) {
			return sys.current_state().pressed(desc->bit_index());
		}
		return false;
	}

	auto released(
		const id action_id
	) -> bool {
		auto& sys = system_of<system>();
		if (const auto* desc = sys.description(action_id)) {
			return sys.current_state().released(desc->bit_index());
		}
		return false;
	}

	auto pressed_mask(
	) -> const mask& {
		return system_of<system>().current_state().pressed_mask();
	}

	auto released_mask(
	) -> const mask& {
		return system_of<system>().current_state().released_mask();
	}

	auto axis1(
		const id axis_id
	) -> float {
		return system_of<system>().current_state().axis1(static_cast<std::uint16_t>(axis_id.number()));
	}

	auto axis2(
		const id axis_id
	) -> axis {
		return system_of<system>().current_state().axis2_v(static_cast<std::uint16_t>(axis_id.number()));
	}

	auto held(
		const state& s,
		const id action_id
	) -> bool {
		if (const auto* desc = system_of<system>().description(action_id)) {
			return s.held(desc->bit_index());
		}
		return false;
	}

	auto pressed(
		const state& s,
		const id action_id
	) -> bool {
		if (const auto* desc = system_of<system>().description(action_id)) {
			return s.pressed(desc->bit_index());
		}
		return false;
	}

	auto released(
		const state& s,
		const id action_id
	) -> bool {
		if (const auto* desc = system_of<system>().description(action_id)) {
			return s.released(desc->bit_index());
		}
		return false;
	}

	auto held(
		const handle& h,
		const state& s
	) -> bool {
		return h.held(s, system_of<system>());
	}

	auto pressed(
		const handle& h,
		const state& s
	) -> bool {
		return h.pressed(s, system_of<system>());
	}

	auto released(
		const handle& h,
		const state& s
	) -> bool {
		return h.released(s, system_of<system>());
	}
}