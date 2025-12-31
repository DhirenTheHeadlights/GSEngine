export module gse.runtime:actions_api;

import std;

import gse.utility;
import gse.physics.math;
import gse.platform;

import :core_api;

export namespace gse::actions {
	template <typename Tag>
	auto add(
		const key default_key
	) -> id {
		return system_of<system>().add<Tag>(default_key);
	}

	auto bind_axis2(
		const pending_axis2_info& info
	) -> id {
		return system_of<system>().bind_axis2(info);
	}

	auto register_channel_binding(
		const id owner,
		std::function<void(const state&)> sampler
	) -> void {
		system_of<system>().register_channel_binding(owner, std::move(sampler));
	}

	auto held(
		const id action_id
	) -> bool {
		auto& s = system_of<system>();
		if (const auto* desc = s.description(action_id)) {
			return s.current_state().held(desc->bit_index());
		}
		return false;
	}

	auto pressed(
		id action_id
	) -> bool {
		auto& s = system_of<system>();
		if (const auto* desc = s.description(action_id)) {
			return s.current_state().pressed(desc->bit_index());
		}
		return false;
	}

	auto released(
		const id action_id
	) -> bool {
		auto& s = system_of<system>();
		if (const auto* desc = s.description(action_id)) {
			return s.current_state().released(desc->bit_index());
		}
		return false;
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

	auto sample_for_entity(
		const id owner_id
	) -> void {
		const auto& s = system_of<system>();
		s.sample_for_entity(s.current_state(), owner_id);
	}
}