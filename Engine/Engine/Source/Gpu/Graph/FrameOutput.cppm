export module gse.gpu:frame_output;

import std;

import gse.core;
import gse.gpu_backend;
import gse.math;
import gse.os;

import :device;
import :frame;
import :image;
import :swap_chain;

export namespace gse::gpu {
	struct offscreen_output_desc {
		vec2u extent;
		std::uint32_t slots = 1;
		bool exportable = false;
	};

	struct offscreen_timelines {
		handle<semaphore> produced;
		handle<semaphore> consumed;
	};

	struct exported_output {
		vec2u extent;
		image_format format = image_format::r8g8b8a8_unorm;
		std::span<const shared_surface> surfaces;
		void* produced_semaphore = nullptr;
		void* consumed_semaphore = nullptr;
	};
}

namespace gse::gpu {
	struct no_output {
		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto color_target() const -> const image*;

		[[nodiscard]] auto live() const -> bool;

		[[nodiscard]] auto target() const -> image_ref;
	};

	struct present_output {
		swap_chain* swapchain = nullptr;
		const frame* owner = nullptr;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto color_target() const -> const image*;

		[[nodiscard]] auto live() const -> bool;

		[[nodiscard]] auto target() const -> image_ref;
	};

	struct offscreen_output {
		std::vector<image> images;
		std::vector<shared_surface> shared;
		offscreen_timelines timelines;
		os::unique_handle produced_export;
		os::unique_handle consumed_export;
		image withheld_target;
		std::uint64_t published = 0;
		bool withheld = false;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto color_target() const -> const image*;

		[[nodiscard]] auto live() const -> bool;

		[[nodiscard]] auto target() const -> image_ref;

		[[nodiscard]] auto frame_number() const -> std::uint64_t;

		[[nodiscard]] auto exportable() const -> bool;

		[[nodiscard]] auto exported() const -> exported_output;
	};

	using frame_output = std::variant<no_output, present_output, offscreen_output>;

	auto create_offscreen_output(
		device& dev,
		const offscreen_output_desc& desc
	) -> std::expected<offscreen_output, std::string>;

	auto destroy_offscreen_output(
		device& dev,
		offscreen_output& output
	) -> void;
}

auto gse::gpu::no_output::extent() const -> vec2u {
	return {};
}

auto gse::gpu::no_output::color_target() const -> const image* {
	return nullptr;
}

auto gse::gpu::no_output::live() const -> bool {
	return false;
}

auto gse::gpu::no_output::target() const -> image_ref {
	return {};
}

auto gse::gpu::present_output::extent() const -> vec2u {
	return swapchain->extent();
}

auto gse::gpu::present_output::color_target() const -> const image* {
	return nullptr;
}

auto gse::gpu::present_output::live() const -> bool {
	return owner->targets().front().acquired;
}

auto gse::gpu::present_output::target() const -> image_ref {
	return {
		.image = swapchain->image(owner->image_index()),
		.extent = swapchain->extent(),
		.format = swapchain->format(),
	};
}

auto gse::gpu::offscreen_output::extent() const -> vec2u {
	const auto ext = images.front().extent();
	return { ext.x(), ext.y() };
}

auto gse::gpu::offscreen_output::color_target() const -> const image* {
	if (withheld) {
		return std::addressof(withheld_target);
	}
	return std::addressof(images[published % images.size()]);
}

auto gse::gpu::offscreen_output::live() const -> bool {
	return true;
}

auto gse::gpu::offscreen_output::target() const -> image_ref {
	const auto& img = *color_target();
	return {
		.image = img.handle(),
		.extent = extent(),
		.format = img.format(),
	};
}

auto gse::gpu::offscreen_output::frame_number() const -> std::uint64_t {
	return published;
}

auto gse::gpu::offscreen_output::exportable() const -> bool {
	return !shared.empty();
}

auto gse::gpu::offscreen_output::exported() const -> exported_output {
	return {
		.extent = extent(),
		.format = images.front().format(),
		.surfaces = shared,
		.produced_semaphore = produced_export.handle(),
		.consumed_semaphore = consumed_export.handle(),
	};
}

auto gse::gpu::create_offscreen_output(device& dev, const offscreen_output_desc& desc) -> std::expected<offscreen_output, std::string> {
	const auto format = dev.surface_format();
	offscreen_output output;
	output.images.reserve(desc.slots);

	if (!desc.exportable) {
		for (std::uint32_t i = 0; i < desc.slots; ++i) {
			auto img = dev.create_image(
				{
					.size = desc.extent,
					.format = format,
					.usage = { image_flag::color_attachment, image_flag::sampled, image_flag::transfer_src },
				},
				"offscreen.color"
			);
			transition_image_to(dev, img);
			output.images.push_back(std::move(img));
		}
		return output;
	}

	output.shared.reserve(desc.slots);
	for (std::uint32_t i = 0; i < desc.slots; ++i) {
		auto surface = dev.create_shared_surface({
			.extent = desc.extent,
			.format = format,
		});
		if (!surface) {
			destroy_offscreen_output(dev, output);
			return std::unexpected(std::format("create_shared_surface[{}] failed: {}", i, surface.error()));
		}
		output.shared.push_back(*surface);
		output.images.emplace_back(
			surface->image,
			surface->view,
			format,
			vec3u{ desc.extent.x(), desc.extent.y(), 1 },
			image_view_create_info{
				.format = format,
				.view_type = image_view_type::e2d,
				.aspects = image_aspect_flags(image_aspect_flag::color),
				.level_count = 1,
				.layer_count = 1,
			}
		);
	}
	output.withheld_target = dev.create_image(
		{
			.size = desc.extent,
			.format = format,
			.usage = { image_flag::color_attachment, image_flag::sampled, image_flag::transfer_src },
		},
		"offscreen.withheld"
	);
	transition_image_to(dev, output.withheld_target);
	output.timelines = {
		.produced = dev.create_exportable_semaphore(),
		.consumed = dev.create_exportable_semaphore(),
	};

	const auto produced_export = dev.export_semaphore_handle(output.timelines.produced);
	if (!produced_export) {
		destroy_offscreen_output(dev, output);
		return std::unexpected(std::format("produced semaphore export failed: {}", produced_export.error()));
	}
	output.produced_export = os::unique_handle(*produced_export);

	const auto consumed_export = dev.export_semaphore_handle(output.timelines.consumed);
	if (!consumed_export) {
		destroy_offscreen_output(dev, output);
		return std::unexpected(std::format("consumed semaphore export failed: {}", consumed_export.error()));
	}
	output.consumed_export = os::unique_handle(*consumed_export);
	return output;
}

auto gse::gpu::destroy_offscreen_output(device& dev, offscreen_output& output) -> void {
	output.images.clear();
	output.withheld_target = {};
	for (const auto& surface : output.shared) {
		dev.destroy_shared_surface(surface);
	}
	output.shared.clear();
	if (output.timelines.produced) {
		dev.retire(output.timelines.produced);
	}
	if (output.timelines.consumed) {
		dev.retire(output.timelines.consumed);
	}
	output.timelines = {};
	output.produced_export = {};
	output.consumed_export = {};
}
