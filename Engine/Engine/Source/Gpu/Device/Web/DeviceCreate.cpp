module gse.gpu:device_create_impl;

import gse.concurrency;
import gse.ecs;
import gse.gpu_backend;
import gse.meta;
import gse.os;
import std;

import :backend_state;
import :device;
import :device_contract;
import :device_webgpu_backend;
import :video_encoder_impl;

auto gse::gpu::device::create(const std::optional<shared_view<window::data>> win, bool, backend_kind& backend, device_settings&) -> std::unique_ptr<device> {
	static constexpr gpu_dispatch dispatch = meta::build_dispatch<gpu_dispatch, device_backend_contract, meta::pointer_receiver<webgpu_device_backend>>();
	auto created = create_webgpu_device_backend(win);
	backend = backend_kind::webgpu;
	active_backend = backend_kind::webgpu;

	std::unique_ptr<void, void (*)(void*)> backend_ptr(
		created.backend.release(),
		[](void* self) {
			delete static_cast<webgpu_device_backend*>(self);
		}
	);

	auto dev = std::unique_ptr<device>(new device(
		std::move(backend_ptr),
		&dispatch,
		created.commands,
		created.surface_format,
		false,
		false
	));

	dev->m_transient = transient_executor<device>::create(
		*dev,
		dev->queue_family(queue_type::graphics),
		dev->queue_family(queue_type::compute),
		task::thread_count()
	);

	return dev;
}
