export module gse.graphics:renderer;

import std;

import :lighting_renderer;
import :geometry_renderer;
import :sprite_renderer;
import :text_renderer;
import :shadow_renderer;
import :physics_debug_renderer;
import :gui;
import :render_component;
import :resource_loader;
import :material;
import :rendering_context;
import :rendering_stack;

import gse.utility;
import gse.platform;

namespace gse {
	renderer::context rendering_context("GSEngine");
}

export namespace gse {
	template <typename Resource>
	auto get(const id id) -> resource::handle<Resource> {
		return rendering_context.get<Resource>(id);
	}

	template <typename Resource>
	auto get(const std::string& filename) -> resource::handle<Resource> {
		return rendering_context.get<Resource>(filename);
	}

	template <typename Resource, typename... Args>
	auto queue(const std::string& name, Args&&... args) -> resource::handle<Resource> {
		return rendering_context.queue<Resource>(name, std::forward<Args>(args)...);
	}

	template <typename Resource>
	auto instantly_load(id id) -> resource::handle<Resource> {
		return rendering_context.instantly_load<Resource>(id);
	}

	template <typename Resource>
	auto add(Resource&& resource) -> void {
		rendering_context.add<Resource>(std::forward<Resource>(resource));
	}

	template <typename Resource>
	auto resource_state(const id id) -> resource::state {
		return rendering_context.resource_state<Resource>(id);
	}
}

export namespace gse::renderer {
	auto initialize() -> void;
	auto update(const std::vector<std::reference_wrapper<registry>>& registries) -> void;
	auto render(
		const std::vector<std::reference_wrapper<registry>>& registries,
		const std::function<void()>& in_frame = {}
	) -> void;
	auto shutdown() -> void;
	auto camera() -> camera&;
	auto set_ui_focus(bool focus) -> void;
}

auto gse::renderer::initialize() -> void {
	rendering_context.add_loader<texture>();
	rendering_context.add_loader<model>();
	rendering_context.add_loader<shader>();
	rendering_context.add_loader<font>();
	rendering_context.add_loader<material>();

	rendering_context.compile();

	add_renderer<geometry>(rendering_context);
	add_renderer<shadow>(rendering_context);
	add_renderer<lighting>(rendering_context);
	add_renderer<sprite>(rendering_context);
	add_renderer<text>(rendering_context);
	add_renderer<physics_debug>(rendering_context);

	for (const auto& renderer : renderers()) {
		renderer->initialize();
	}

	gui::initialize(rendering_context);
}

auto gse::renderer::update(const std::vector<std::reference_wrapper<registry>>& registries) -> void {
	rendering_context.process_resource_queue();
	gui::update(rendering_context.window());
	rendering_context.window().update(rendering_context.ui_focus());
	rendering_context.camera().update_orientation();

	if (!rendering_context.ui_focus()) {
		rendering_context.camera().process_mouse_movement(mouse::delta());
	}

	for (const auto& renderer : renderers()) {
		task::post(
			[registries, ptr = renderer.get()] {
				ptr->update(registries);
			}, 
			find_or_generate_id(typeid(*renderer).name())
		);
	}
}

auto gse::renderer::render(const std::vector<std::reference_wrapper<registry>>& registries, const std::function<void()>& in_frame) -> void {
	rendering_context.process_gpu_queue();

	vulkan::frame({
		.window = rendering_context.window().raw_handle(),
		.frame_buffer_resized = rendering_context.window().frame_buffer_resized(),
		.minimized = rendering_context.window().minimized(),
		.config = rendering_context.config(),
		.in_frame = [&registries, &in_frame] {
			auto& cfg = rendering_context.config();
			auto& frame_ctx = cfg.frame_context();
			auto& swap = cfg.swap_chain_config();

			vk::ImageMemoryBarrier2 color_barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = swap.images[frame_ctx.image_index],
				.subresourceRange = {
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			vk::ImageMemoryBarrier2 depth_barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = swap.depth_image.image,
				.subresourceRange = {
					.aspectMask = vk::ImageAspectFlagBits::eDepth,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			std::array barriers{
				color_barrier,
				depth_barrier
			};

			const vk::DependencyInfo begin_dep{
				.imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
				.pImageMemoryBarriers = barriers.data()
			};

			frame_ctx.command_buffer.pipelineBarrier2(begin_dep);

			for (const auto& renderer : renderers()) {
				renderer->render(registries);
			}

			vk::ImageMemoryBarrier2 present_barrier{
				.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
				.dstAccessMask = {},
				.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
				.newLayout = vk::ImageLayout::ePresentSrcKHR,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = rendering_context.config().swap_chain_config().images[rendering_context.config().frame_context().image_index],
				.subresourceRange = {
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			const vk::DependencyInfo dependency_info{
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &present_barrier
			};

			rendering_context.config().frame_context().command_buffer.pipelineBarrier2(dependency_info);

			gui::frame(
				rendering_context,
				renderer<sprite>(),
				renderer<text>(),
				[&] {
					in_frame();
				}
			);
		}
	});

	vulkan::transient_allocator::end_frame();
}

auto gse::renderer::shutdown() -> void {
	gui::save();

	rendering_context.config().device_config().device.waitIdle();

	for (auto& renderer : renderers()) {
		renderer.reset();
	}

	rendering_context.shutdown();

	vulkan::persistent_allocator::clean_up();
	shader::destroy_global_layouts();
}

auto gse::renderer::camera() -> gse::camera& {
	return rendering_context.camera();
}

auto gse::renderer::set_ui_focus(const bool focus) -> void {
	rendering_context.set_ui_focus(focus);
}
