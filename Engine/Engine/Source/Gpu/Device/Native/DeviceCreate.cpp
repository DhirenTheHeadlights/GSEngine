module gse.gpu:device_create_impl;

import gse.concurrency;
import gse.ecs;
import gse.gpu_backend;
import gse.log;
import gse.math;
import gse.meta;
import gse.os;
import std;

import :backend_state;
import :device;
import :device_contract;
import :device_dx12_backend;
import :device_vulkan_backend;

namespace gse::gpu {
	template <typename B>
	auto device_backend_delete(void* self) -> void {
		delete static_cast<B*>(self);
	}

	template <typename B>
	[[nodiscard]] auto device_dispatch_for() -> const gpu_dispatch*;
}

template <typename B>
auto gse::gpu::device_dispatch_for() -> const gpu_dispatch* {
	static constexpr gpu_dispatch dispatch = meta::build_dispatch<gpu_dispatch, device_backend_contract, meta::pointer_receiver<B>>();
	return &dispatch;
}

auto gse::gpu::device::create(const std::optional<shared_view<window::data>> win, const bool validation_layers_enabled, backend_kind& backend, device_settings& device_cfg) -> std::unique_ptr<device> {
	if (backend == backend_kind::vulkan) {
		auto created = create_vulkan_device_backend(win, validation_layers_enabled, device_cfg);
		if (created) {
			active_backend = backend_kind::vulkan;

			std::unique_ptr<void, void (*)(void*)> backend(
				created->backend.release(),
				&device_backend_delete<vulkan_device_backend>
			);

			auto dev = std::unique_ptr<device>(new device(
				std::move(backend),
				device_dispatch_for<vulkan_device_backend>(),
				created->commands,
				created->surface_format,
				created->video_encode_enabled,
				device_cfg.pass_checkpoints
			));

			dev->m_transient = transient_executor<device>::create(
				*dev,
				dev->queue_family(queue_type::graphics),
				dev->queue_family(queue_type::compute),
				task::thread_count()
			);

			return dev;
		}

		log::println(log::category::render, "vulkan device unavailable; falling back to dx12 backend");
		log::flush();
		backend = backend_kind::dx12;
	}

	auto created = create_dx12_device_backend(win, validation_layers_enabled, device_cfg);
	active_backend = backend_kind::dx12;

	std::unique_ptr<void, void (*)(void*)> backend_ptr(
		created.backend.release(),
		&device_backend_delete<dx12_device_backend>
	);

	auto dev = std::unique_ptr<device>(new device(
		std::move(backend_ptr),
		device_dispatch_for<dx12_device_backend>(),
		created.commands,
		created.surface_format,
		created.video_encode_enabled,
		device_cfg.pass_checkpoints
	));

	dev->m_transient = transient_executor<device>::create(
		*dev,
		dev->queue_family(queue_type::graphics),
		dev->queue_family(queue_type::compute),
		task::thread_count()
	);

	return dev;
}
