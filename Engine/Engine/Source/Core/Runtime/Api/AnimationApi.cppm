export module gse.runtime:animation_api;

import std;

import gse.utility;
import gse.graphics;

import :renderer_api;

export namespace gse::animation {
	auto create_skeleton(
		const skeleton::params& p
	) -> id {
		auto s = skeleton(p);
		const auto skel_id = s.id();
		add(std::move(s));
		return skel_id;
	}

	auto create_clip(
		const clip_asset::params& p
	) -> id {
		auto c = clip_asset(p);
		const auto clip_id = c.id();
		add(std::move(c));
		return clip_id;
	}
}
