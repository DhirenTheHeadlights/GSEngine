module gse.os:window_impl;

import std;

import gse.core;
import gse.ecs;
import gse.math;
import gse.time;

import :window;

auto gse::window::tick(scheduler&, data&) -> void {}

auto gse::window::shutdown(data& d) -> void {
	d.secondaries.clear();
}

auto gse::window::wait_events(time) -> void {}

auto gse::window::install_resize_pump(std::function<void()>) -> void {}

auto gse::window::post_wake() -> void {}

auto gse::window::show(data&) -> void {}

auto gse::window::minimized(native_window_handle) -> bool {
	return true;
}

auto gse::window::viewport(native_window_handle) -> vec2i {
	return { 0, 0 };
}
