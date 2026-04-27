export module gse.gpu:context;

import std;
import vulkan;

import :types;
import :vulkan_runtime;
import :vulkan_uploader;
import :vulkan_reflect;
import :descriptor_heap;
import :shader;
import :shader_registry;
import :device;
import :swap_chain;
import :frame;
import :render_graph;
import :bindless;

import gse.os;
import gse.config;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.save;

export namespace gse {
	enum class render_layer : std::uint8_t {
		background = 0,
		content = 1,
		overlay = 2,
		popup = 3,
		modal = 4,
		cursor = 5,
		debug = 6
	};
}

export namespace gse::gpu {
	class context final : public non_copyable {
	public:
		explicit context(
			const std::string& window_title,
			input::system_state& input,
			save::state& save
		);
		~context() override;

		using command = std::function<void(context&)>;

		template <typename Resource, typename Fn>
		auto queue_gpu_command(
			Resource* resource,
			Fn&& work
		) const -> void;

		auto process_gpu_queue(
		) -> void;

		auto begin_frame(
		) -> std::expected<frame_token, frame_status>;

		auto end_frame(
		) -> void;

		[[nodiscard]] auto gpu_queue_size(
		) const -> std::size_t;

		auto mark_pending_for_finalization(
			id resource_type,
			id resource_id
		) const -> void;

		auto take_pending_finalizations(
		) const -> std::vector<std::pair<id, id>>;

		[[nodiscard]] auto descriptor_heap(
			this auto& self
		) -> decltype(auto);

		auto add_transient_work(
			auto&& commands
		) -> void;

		using swap_chain_recreate_callback = std::function<void()>;
		auto on_swap_chain_recreate(
			swap_chain_recreate_callback callback
		) const -> void;

		[[nodiscard]] auto window(
		) -> window&;

		[[nodiscard]] auto window(
		) const -> const gse::window&;

		[[nodiscard]] auto graph(
		) const -> render_graph&;

		auto set_ui_focus(
			bool focus
		) -> void;

		[[nodiscard]] auto ui_focus(
		) const -> bool;

		auto shutdown(
		) -> void;

		[[nodiscard]] auto scheduler(
			this auto& self
		) -> auto& { return self.m_scheduler; }

		[[nodiscard]] auto device(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto swapchain(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto frame(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto shader_registry(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto allocator(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto logical_device(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto bindless_textures(
			this auto& self
		) -> auto&;

		auto wait_idle(
		) const -> void;

		auto set_asset_registry(
			void* registry
		) -> void;

		[[nodiscard]] auto assets(
		) -> asset_registry<context>&;

		[[nodiscard]] auto try_assets(
		) -> asset_registry<context>*;

	private:
		gse::window m_window;
		void* m_asset_registry = nullptr;
		std::unique_ptr<gpu::device> m_device;
		std::unique_ptr<gpu::shader_registry> m_shader_registry;
		std::unique_ptr<swap_chain> m_swapchain;
		std::unique_ptr<gpu::frame> m_frame;
		std::unique_ptr<render_graph> m_render_graph;
		std::unique_ptr<bindless_texture_set> m_bindless_textures;
		concurrency::frame_scheduler m_scheduler;

		mutable std::vector<command> m_command_queue;
		mutable std::vector<std::pair<id, id>> m_pending_gpu_resources;
		mutable std::recursive_mutex m_mutex;

		bool m_ui_focus = false;
		bool m_validation_layers_enabled = false;
	};
}

gse::gpu::context::context(const std::string& window_title, input::system_state& input, save::state& save)
	: m_window(window_title, input, save)
	, m_device(device::create(m_window, save))
	, m_shader_registry(std::make_unique<gpu::shader_registry>(m_device->logical_device()))
	, m_swapchain(swap_chain::create(m_window.viewport(), *m_device))
	, m_frame(frame::create(*m_device, *m_swapchain))
{
	m_render_graph = std::make_unique<render_graph>(*m_device, *m_swapchain, *m_frame);
	m_bindless_textures = std::make_unique<bindless_texture_set>(m_device->logical_device(), m_device->descriptor_heap());

	save.bind("Graphics", "Validation Layers", m_validation_layers_enabled)
		.description("Enable Vulkan validation layers for debugging (impacts performance significantly)")
		.default_value(false)
		.restart_required()
		.commit();

	auto transition_depth = [this]() {
		auto& depth = m_render_graph->depth_image();
		m_frame->add_transient_work([&depth](const vk::raii::CommandBuffer& cmd) {
			vulkan::uploader::transition_image_layout(
				*cmd,
				depth,
				vulkan::to_vk(image_layout::general),
				vk::ImageAspectFlagBits::eDepth,
				vk::PipelineStageFlagBits2::eTopOfPipe,
				{},
				vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead
			);
		});
	};
	transition_depth();
	m_swapchain->on_recreate([transition_depth]() {
		transition_depth();
	});
}

gse::gpu::context::~context() {
	m_frame.reset();
	m_swapchain.reset();
	m_shader_registry.reset();
	m_device.reset();
}

template <typename Resource, typename Fn>
auto gse::gpu::context::queue_gpu_command(Resource* resource, Fn&& work) const -> void {
	command final_command = [resource, work_lambda = std::forward<Fn>(work)](context& ctx) {
		work_lambda(ctx, *resource);
	};

	std::lock_guard lock(m_mutex);
	m_command_queue.push_back(std::move(final_command));
}

auto gse::gpu::context::process_gpu_queue() -> void {
	std::vector<command> commands_to_run;

	{
		std::lock_guard lock(m_mutex);
		commands_to_run.swap(m_command_queue);
	}

	for (auto& cmd : commands_to_run) {
		if (cmd) {
			cmd(*this);
		}
	}
}

auto gse::gpu::context::begin_frame() -> std::expected<frame_token, frame_status> {
	auto result = m_frame->begin(m_window);

	if (result) {
		m_device->descriptor_heap().begin_frame(result->frame_index);
		m_bindless_textures->begin_frame(result->frame_index);
	}

	return result;
}

auto gse::gpu::context::end_frame() -> void {
	m_frame->end(m_window);
}

auto gse::gpu::context::gpu_queue_size() const -> std::size_t {
	std::lock_guard lock(m_mutex);
	return m_command_queue.size();
}

auto gse::gpu::context::mark_pending_for_finalization(id resource_type, const id resource_id) const -> void {
	std::lock_guard lock(m_mutex);
	m_pending_gpu_resources.emplace_back(resource_type, resource_id);
}

auto gse::gpu::context::take_pending_finalizations() const -> std::vector<std::pair<id, id>> {
	std::lock_guard lock(m_mutex);
	std::vector<std::pair<id, id>> result;
	result.swap(m_pending_gpu_resources);
	return result;
}

auto gse::gpu::context::add_transient_work(auto&& commands) -> void {
	m_frame->add_transient_work(std::forward<decltype(commands)>(commands));
}

auto gse::gpu::context::descriptor_heap(this auto& self) -> decltype(auto) {
	return self.m_device->descriptor_heap();
}

auto gse::gpu::context::on_swap_chain_recreate(swap_chain_recreate_callback callback) const -> void {
	m_swapchain->on_recreate(std::move(callback));
}

auto gse::gpu::context::graph() const -> render_graph& {
	return *m_render_graph;
}

auto gse::gpu::context::window() -> gse::window& {
	return m_window;
}

auto gse::gpu::context::window() const -> const gse::window& {
	return m_window;
}

auto gse::gpu::context::set_ui_focus(const bool focus) -> void {
	m_ui_focus = focus;
}

auto gse::gpu::context::ui_focus() const -> bool {
	return m_ui_focus;
}

auto gse::gpu::context::shutdown() -> void {
	if (!m_device) return;

	m_window.shutdown();
	m_device->wait_idle();
	m_swapchain->clear_depth_image();
}

auto gse::gpu::context::device(this auto& self) -> auto& {
	return *self.m_device;
}

auto gse::gpu::context::swapchain(this auto& self) -> auto& {
	return *self.m_swapchain;
}

auto gse::gpu::context::frame(this auto& self) -> auto& {
	return *self.m_frame;
}

auto gse::gpu::context::shader_registry(this auto& self) -> auto& {
	return *self.m_shader_registry;
}

auto gse::gpu::context::allocator(this auto& self) -> auto& {
	return self.m_device->allocator();
}

auto gse::gpu::context::logical_device(this auto& self) -> auto& {
	return self.m_device->logical_device();
}

auto gse::gpu::context::bindless_textures(this auto& self) -> auto& {
	return *self.m_bindless_textures;
}

auto gse::gpu::context::wait_idle() const -> void {
	m_device->wait_idle();
}

auto gse::gpu::context::set_asset_registry(void* registry) -> void {
	m_asset_registry = registry;
}

auto gse::gpu::context::assets() -> asset_registry<context>& {
	return *static_cast<asset_registry<context>*>(m_asset_registry);
}

auto gse::gpu::context::try_assets() -> asset_registry<context>* {
	return static_cast<asset_registry<context>*>(m_asset_registry);
}
