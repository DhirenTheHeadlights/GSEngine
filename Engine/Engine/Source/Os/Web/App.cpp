module gse.os:app_impl;

import std;

import :app;

import gse.log;

auto gse::app::start_relaunch(const relaunch_plan&) -> void {
	log::println(
		log::level::warning,
		log::category::general,
		"start_relaunch: relaunching is unsupported on this platform; the request is dropped"
	);
}
