export module gse.gpu:frame_output;

import std;

import gse.core;
import gse.gpu_backend;
import gse.math;

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
		const frame* owner = nullptr;
		std::vector<image> images;
		std::vector<shared_surface> shared;
		offscreen_timelines timelines;
		std::uint64_t base_frame = 0;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto color_target() const -> const image*;

		[[nodiscard]] auto live() const -> bool;

		[[nodiscard]] auto target() const -> image_ref;

		[[nodiscard]] auto frame_number() const -> std::uint64_t;

		[[nodiscard]] auto exportable() const -> bool;
	};

	using frame_output = std::variant<no_output, present_output, offscreen_output>;

	auto create_offscreen_output(
		device& dev,
		const frame& owner,
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
	return std::addressof(images[frame_number() % images.size()]);
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
	return owner->frame_count() - base_frame;
}

auto gse::gpu::offscreen_output::exportable() const -> bool {
	return !shared.empty();
}

auto gse::gpu::create_offscreen_output(device& dev, const frame& owner, const offscreen_output_desc& desc) -> std::expected<offscreen_output, std::string> {
	const auto format = dev.surface_format();
	offscreen_output output{
		.owner = std::addressof(owner),
		.base_frame = owner.frame_count(),
	};
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
	output.timelines = {
		.produced = dev.create_exportable_semaphore(),
		.consumed = dev.create_exportable_semaphore(),
	};
	return output;
}

auto gse::gpu::destroy_offscreen_output(device& dev, offscreen_output& output) -> void {
	output.images.clear();
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
}
