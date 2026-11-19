module gse.gpu:device_create_impl;

import gse.assert;
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
import :device_webgpu_backend;

namespace gse::gpu {
	constexpr std::array fallback_order{
		backend_kind::vulkan,
		backend_kind::dx12,
		backend_kind::webgpu,
	};

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

template <typename B, typename Creation>
auto gse::gpu::device::from_backend(Creation&& created, const bool pass_checkpoints) -> std::unique_ptr<device> {
	std::unique_ptr<void, void (*)(void*)> backend_ptr(
		created.backend.release(),
		&device_backend_delete<B>
	);

	auto dev = std::unique_ptr<device>(new device(
		std::move(backend_ptr),
		device_dispatch_for<B>(),
		created.commands,
		created.surface_format,
		created.video_encode_enabled,
		pass_checkpoints
	));

	dev->m_transient = transient_executor<device>::create(
		*dev,
		dev->queue_family(queue_type::graphics),
		dev->queue_family(queue_type::compute),
		task::thread_count()
	);

	return dev;
}

auto gse::gpu::device::create(const std::optional<shared_view<window::data>> win, const bool validation_layers_enabled, backend_kind& backend, device_settings& device_cfg) -> std::unique_ptr<device> {
	std::vector<backend_kind> candidates{ backend };
	for (const auto kind : fallback_order) {
		if (kind != backend) {
			candidates.push_back(kind);
		}
	}

	std::string failures;
	for (const auto kind : candidates) {
		auto dev = [&] -> expected<std::unique_ptr<device>> {
			switch (kind) {
				case backend_kind::vulkan:
					return create_vulkan_device_backend(win, validation_layers_enabled, device_cfg).transform([&](auto&& created) {
						return from_backend<vulkan_device_backend>(std::move(created), device_cfg.pass_checkpoints);
					});
				case backend_kind::dx12:
					return create_dx12_device_backend(win, validation_layers_enabled, device_cfg).transform([&](auto&& created) {
						return from_backend<dx12_device_backend>(std::move(created), device_cfg.pass_checkpoints);
					});
				case backend_kind::webgpu:
					return create_webgpu_device_backend(win).transform([](auto&& created) {
						return from_backend<webgpu_device_backend>(std::move(created), false);
					});
			}
			std::unreachable();
		}();

		if (dev) {
			if (kind != backend) {
				log::println(log::level::warning, log::category::render, "{} backend unavailable; running on {}", backend, kind);
			}
			backend = kind;
			active_backend = kind;
			return std::move(*dev);
		}

		log::println(log::level::warning, log::category::render, "{} backend unavailable ({})", kind, dev.error());
		log::flush();
		failures += std::format(" {}: {};", kind, dev.error());
	}

	assert(false, "no gpu backend could create a device:{}", failures);
	std::unreachable();
}
