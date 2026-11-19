module gse.os:window_shared_impl;

import std;

import gse.core;

import :window;

auto gse::window::frame_buffer_resized(window_surface& s) -> bool {
	if (s.framebuffer_resized) {
		s.framebuffer_resized = false;
		return true;
	}
	return false;
}

auto gse::window::find_surface(data& d, const id id) -> window_surface* {
	if (!id.exists() || d.primary.id == id) {
		return &d.primary;
	}
	const auto it = std::ranges::find_if(d.secondaries, [id](const auto& held) {
		return held->id == id;
	});
	return it == d.secondaries.end() ? nullptr : it->get();
}
