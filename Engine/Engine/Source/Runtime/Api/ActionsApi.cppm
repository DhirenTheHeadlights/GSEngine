export module gse.runtime:actions_api;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.os;
import gse.assets;
import gse.gpu;

import :core_api;

namespace gse::actions {
	auto add_by_name(
		std::string_view tag,
		key default_key
	) -> handle;
}

export namespace gse::actions {
	template <fixed_string Tag>
	auto add(
		key default_key
	) -> handle;

	auto bind_axis2(
		const pending_axis2_info& info
	) -> id;

	template <fixed_string Tag>
	auto bind_button_channel(
		const id owner_id,
		const key default_key,
		button_channel& channel
	) -> void;

	auto bind_axis2_channel(
		const id owner_id,
		const pending_axis2_info& info,
		axis2_channel& channel,
		const id axis_id
	) -> void;

	auto register_channel(
		const id owner_id,
		button_channel& channel
	) -> void;

	auto register_channel(
		const id owner_id,
		axis1_channel& channel
	) -> void;

	auto register_channel(
		const id owner_id,
		axis2_channel& channel
	) -> void;

	auto sample_for_entity(
		const state& s,
		const id owner_id
	) -> void;

	auto sample_all_channels(
		const state& s
	) -> void;

	auto current_state(
	) -> const state&;

	auto axis1_ids(
	) -> std::span<const std::uint16_t>;

	auto axis2_ids(
	) -> std::span<const std::uint16_t>;

	auto held(
		const id action_id
	) -> bool;

	auto pressed(
		const id action_id
	) -> bool;

	auto released(
		const id action_id
	) -> bool;

	auto pressed_mask(
	) -> const mask&;

	auto released_mask(
	) -> const mask&;

	auto axis1(
		const id axis_id
	) -> float;

	auto axis2(
		const id axis_id
	) -> axis;

	auto held(
		const state& s,
		const id action_id
	) -> bool;

	auto pressed(
		const state& s,
		const id action_id
	) -> bool;

	auto released(
		const state& s,
		const id action_id
	) -> bool;

	auto held(
		const handle& h,
		const state& s
	) -> bool;

	auto pressed(
		const handle& h,
		const state& s
	) -> bool;

	auto released(
		const handle& h,
		const state& s
	) -> bool;
}

auto gse::actions::add_by_name(const std::string_view tag, const key default_key) -> handle {
	const id action_id = generate_id(tag);

	channel_add(add_action_request{
		.name = std::string(tag),
		.default_key = default_key,
		.action_id = action_id
	});

	return handle(action_id);
}

template <gse::fixed_string Tag>
auto gse::actions::add(const key default_key) -> handle {
	return add_by_name(Tag, default_key);
}

auto gse::actions::bind_axis2(const pending_axis2_info& info) -> id {
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

template <gse::fixed_string Tag>
auto gse::actions::bind_button_channel(const id owner_id, const key default_key, button_channel& channel) -> void {
	auto h = add<Tag>(default_key);
	channel.action_id = h.id();
	defer([owner_id, &channel](system::state& s) {
		system::register_channel(s, owner_id, channel);
	});
}

auto gse::actions::bind_axis2_channel(const id owner_id, const pending_axis2_info& info, axis2_channel& channel, const id axis_id) -> void {
	channel.axis_id = axis_id;
	defer([owner_id, &channel](system::state& s) {
		system::register_channel(s, owner_id, channel);
	});

	channel_add(bind_axis2_request{
		.info = info,
		.axis_id = axis_id
	});
}

auto gse::actions::register_channel(const id owner_id, button_channel& channel) -> void {
	defer([owner_id, &channel](system::state& s) {
		system::register_channel(s, owner_id, channel);
	});
}

auto gse::actions::register_channel(const id owner_id, axis1_channel& channel) -> void {
	defer([owner_id, &channel](system::state& s) {
		system::register_channel(s, owner_id, channel);
	});
}

auto gse::actions::register_channel(const id owner_id, axis2_channel& channel) -> void {
	defer([owner_id, &channel](system::state& s) {
		system::register_channel(s, owner_id, channel);
	});
}

auto gse::actions::sample_for_entity(const state& s, const id owner_id) -> void {
	defer([&s, owner_id](system::state& sys) {
		system::sample_for_entity(sys, s, owner_id);
	});
}

auto gse::actions::sample_all_channels(const state& s) -> void {
	defer([&s](system::state& sys) {
		system::sample_all_channels(sys, s);
	});
}

auto gse::actions::current_state() -> const state& {
	return system::current_state(state_of<system::state>());
}

auto gse::actions::axis1_ids() -> std::span<const std::uint16_t> {
	return system::axis1_ids(state_of<system::state>());
}

auto gse::actions::axis2_ids() -> std::span<const std::uint16_t> {
	return system::axis2_ids(state_of<system::state>());
}

auto gse::actions::held(const id action_id) -> bool {
	const auto& sys = state_of<system::state>();
	if (const auto* desc = system::description(sys, action_id)) {
		return system::current_state(sys).held(desc->bit_index());
	}
	return false;
}

auto gse::actions::pressed(const id action_id) -> bool {
	const auto& sys = state_of<system::state>();
	if (const auto* desc = system::description(sys, action_id)) {
		return system::current_state(sys).pressed(desc->bit_index());
	}
	return false;
}

auto gse::actions::released(const id action_id) -> bool {
	const auto& sys = state_of<system::state>();
	if (const auto* desc = system::description(sys, action_id)) {
		return system::current_state(sys).released(desc->bit_index());
	}
	return false;
}

auto gse::actions::pressed_mask() -> const mask& {
	return system::current_state(state_of<system::state>()).pressed_mask();
}

auto gse::actions::released_mask() -> const mask& {
	return system::current_state(state_of<system::state>()).released_mask();
}

auto gse::actions::axis1(const id axis_id) -> float {
	return system::current_state(state_of<system::state>()).axis1(static_cast<std::uint16_t>(axis_id.number()));
}

auto gse::actions::axis2(const id axis_id) -> axis {
	return system::current_state(state_of<system::state>()).axis2_v(static_cast<std::uint16_t>(axis_id.number()));
}

auto gse::actions::held(const state& s, const id action_id) -> bool {
	if (const auto* desc = system::description(state_of<system::state>(), action_id)) {
		return s.held(desc->bit_index());
	}
	return false;
}

auto gse::actions::pressed(const state& s, const id action_id) -> bool {
	if (const auto* desc = system::description(state_of<system::state>(), action_id)) {
		return s.pressed(desc->bit_index());
	}
	return false;
}

auto gse::actions::released(const state& s, const id action_id) -> bool {
	if (const auto* desc = system::description(state_of<system::state>(), action_id)) {
		return s.released(desc->bit_index());
	}
	return false;
}

auto gse::actions::held(const handle& h, const state& s) -> bool {
	return system::held(s, state_of<system::state>(), h);
}

auto gse::actions::pressed(const handle& h, const state& s) -> bool {
	return system::pressed(s, state_of<system::state>(), h);
}

auto gse::actions::released(const handle& h, const state& s) -> bool {
	return system::released(s, state_of<system::state>(), h);
}
