export module gse.physics:body_requests;

import std;

import gse.core;
import gse.ecs;
import gse.math;

import :collision_component;
import :motion_component;
import :transform_component;

export namespace gse::physics {
	consteval auto settable_members() -> std::vector<std::meta::info>;

	consteval auto is_settable_member(
		std::meta::info member
	) -> bool;

	consteval auto set_request_channel(
		std::meta::info channel_template
	) -> std::meta::info;

	template <std::meta::info Member>
		requires (is_settable_member(Member))
	struct set_request {
		id target;
		typename [:std::meta::type_of(Member):] value;
	};
}

consteval auto gse::physics::settable_members() -> std::vector<std::meta::info> {
	std::vector<std::meta::info> out;
	for (const auto component : { ^^transform_component, ^^motion_component, ^^collision_component }) {
		for (const auto member : std::meta::nonstatic_data_members_of(component, std::meta::access_context::unchecked())) {
			if (member == ^^motion_component::reset_pending) {
				continue;
			}
			out.push_back(member);
		}
	}
	return out;
}

consteval auto gse::physics::is_settable_member(const std::meta::info member) -> bool {
	return std::ranges::contains(settable_members(), member);
}

consteval auto gse::physics::set_request_channel(const std::meta::info channel_template) -> std::meta::info {
	std::vector<std::meta::info> requests;
	for (const auto member : settable_members()) {
		requests.push_back(std::meta::substitute(^^set_request, { std::meta::reflect_constant(member) }));
	}
	return std::meta::substitute(channel_template, requests);
}

export namespace gse::physics {
	using set_request_read = [:set_request_channel(^^channel_read):];
}
