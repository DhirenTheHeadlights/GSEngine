module gse.gpu:device_create_impl;

import gse.ecs;
import gse.gpu_backend;
import gse.log;
import gse.os;
import std;

import :device;

auto gse::gpu::device::create(std::optional<shared_view<window::data>>, bool, backend_kind&, device_settings&) -> std::unique_ptr<device> {
	log::println(log::level::error, log::category::render, "no gpu backend is compiled into this configuration; device creation is unsupported");
	log::flush();
	return nullptr;
}
