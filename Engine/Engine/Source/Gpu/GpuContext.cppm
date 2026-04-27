export module gse.gpu.context:gpu_context;

import std;
import vulkan;

import gse.gpu.shader;
import gse.gpu.device;
import gse.gpu.types;
import gse.gpu.vulkan;
import :bindless;

import gse.os;
import gse.assets;
import gse.config;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
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

		template <typename T>
		auto add_loader(
		) -> resource::loader<T, context>*;

		template <typename T>
		auto get(
			id id
		) const -> resource::handle<T>;

		template <typename T>
		auto get(
			const std::string& filename
		) const -> resource::handle<T>;

		template <typename T>
		auto try_get(
			id id
		) const -> resource::handle<T>;

		template <typename T>
		auto try_get(
			const std::string& filename
		) const -> resource::handle<T>;

		template <typename T, typename... Args>
		auto queue(
			const std::string& name,
			Args&&... args
		) -> resource::handle<T>;

		template <typename Resource, typename Fn>
		auto queue_gpu_command(
			Resource* resource,
			Fn&& work
		) const -> void;

		template <typename T>
		auto instantly_load(
			const resource::handle<T>& handle
		) -> void;

		template <typename T>
		auto add(
			T&& resource
		) -> resource::handle<T>;

		auto process_resource_queue(
		) -> void;

		auto process_gpu_queue(
		) -> void;

		auto compile(
		) -> void;

		auto poll_assets(
		) -> void;

		auto enable_hot_reload(
		) -> void;

		auto disable_hot_reload(
		) -> void;

		[[nodiscard]] auto hot_reload_enabled(
		) const -> bool;

		auto finalize_reloads(
		) -> void;

		template <typename T>
		[[nodiscard]] auto resource_state(
			id id
		) const -> resource::state;

		template <typename T>
		[[nodiscard]] auto loader(
			this auto&& self
		) -> decltype(auto);

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
		) const -> vulkan::render_graph&;

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

		[[nodiscard]] static auto enumerate_resources(
			const std::string& baked_dir,
			const std::string& baked_ext
		) -> std::vector<std::string>;

	private:
		auto loader(
			id type_index
		) const -> resource::loader_base*;

		gse::window m_window;
		std::unique_ptr<gpu::device> m_device;
		std::unique_ptr<gpu::shader_registry> m_shader_registry;
		std::unique_ptr<swap_chain> m_swapchain;
		std::unique_ptr<gpu::frame> m_frame;
		std::unique_ptr<vulkan::render_graph> m_render_graph;
		std::unique_ptr<bindless_texture_set> m_bindless_textures;
		concurrency::frame_scheduler m_scheduler;

		asset_pipeline m_pipeline{ config::resource_path, config::baked_resource_path };
		std::unordered_map<id, std::unique_ptr<resource::loader_base>> m_resource_loaders;

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
	m_render_graph = std::make_unique<vulkan::render_graph>(*m_device, *m_swapchain, *m_frame);
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

template <typename T>
auto gse::gpu::context::add_loader() -> resource::loader<T, context>* {
	const auto type_id = id_of<T>();
	assert(
		!m_resource_loaders.contains(type_id),
		std::source_location::current(),
		"Resource loader for type {} already exists.",
		type_tag<T>()
	);

	auto new_loader = std::make_unique<resource::loader<T, context>>(*this);
	auto* loader_ptr = new_loader.get();
	m_resource_loaders[type_id] = std::move(new_loader);

	if constexpr (has_asset_compiler<T>) {
		m_pipeline.register_type<T, context>(loader_ptr);
	}

	return loader_ptr;
}

template <typename T>
auto gse::gpu::context::get(id id) const -> resource::handle<T> {
	return loader<T>()->get(id);
}

template <typename T>
auto gse::gpu::context::get(const std::string& filename) const -> resource::handle<T> {
	return loader<T>()->get(filename);
}

template <typename T>
auto gse::gpu::context::try_get(id id) const -> resource::handle<T> {
	return loader<T>()->try_get(id);
}

template <typename T>
auto gse::gpu::context::try_get(const std::string& filename) const -> resource::handle<T> {
	return loader<T>()->try_get(filename);
}

template <typename T, typename... Args>
auto gse::gpu::context::queue(const std::string& name, Args&&... args) -> resource::handle<T> {
	return loader<T>()->queue(name, std::forward<Args>(args)...);
}

template <typename Resource, typename Fn>
auto gse::gpu::context::queue_gpu_command(Resource* resource, Fn&& work) const -> void {
	command final_command = [resource, work_lambda = std::forward<Fn>(work)](context& ctx) {
		work_lambda(ctx, *resource);
	};

	std::lock_guard lock(m_mutex);
	m_command_queue.push_back(std::move(final_command));
}

template <typename T>
auto gse::gpu::context::instantly_load(const resource::handle<T>& handle) -> void {
	loader<T>()->instantly_load(handle.id());
}

template <typename T>
auto gse::gpu::context::add(T&& resource) -> resource::handle<T> {
	return loader<T>()->add(std::forward<T>(resource));
}

auto gse::gpu::context::process_resource_queue() -> void {
	for (const auto& l : m_resource_loaders | std::views::values) {
		l->flush();
	}
}

auto gse::gpu::context::process_gpu_queue() -> void {
	std::vector<command> commands_to_run;
	std::vector<std::pair<id, id>> resources_to_finalize;

	{
		std::lock_guard lock(m_mutex);
		commands_to_run.swap(m_command_queue);
		resources_to_finalize.swap(m_pending_gpu_resources);
	}

	for (auto& cmd : commands_to_run) {
		if (cmd) {
			cmd(*this);
		}
	}

	for (const auto& [type, resource_id] : resources_to_finalize) {
		loader(type)->update_state(resource_id, resource::state::loaded);
	}
}

auto gse::gpu::context::compile() -> void {
	m_pipeline.register_compiler_only<shader_layout>();

	if (const auto result = m_pipeline.compile_all(); result.success_count > 0 || result.failure_count > 0) {
		log::println(
			result.failure_count > 0 ? log::level::warning : log::level::info,
			log::category::assets,
			"Compiled {} assets ({} skipped, {} failed)",
			result.success_count, result.skipped_count, result.failure_count
		);
	}

	m_shader_registry->load_layouts(config::baked_resource_path / "Layouts");
}

auto gse::gpu::context::poll_assets() -> void {
	m_pipeline.poll();
}

auto gse::gpu::context::enable_hot_reload() -> void {
	m_pipeline.enable_hot_reload();
	log::println(log::category::assets, "Hot reload enabled");
}

auto gse::gpu::context::disable_hot_reload() -> void {
	m_pipeline.disable_hot_reload();
	log::println(log::category::assets, "Hot reload disabled");
}

auto gse::gpu::context::hot_reload_enabled() const -> bool {
	return m_pipeline.hot_reload_enabled();
}

auto gse::gpu::context::finalize_reloads() -> void {
	for (const auto& l : m_resource_loaders | std::views::values) {
		l->finalize_reloads();
	}
}

template <typename T>
auto gse::gpu::context::resource_state(const id id) const -> resource::state {
	return loader<T>()->state_of(id);
}

template <typename T>
auto gse::gpu::context::loader(this auto&& self) -> decltype(auto) {
	auto* base_loader = self.loader(id_of<T>());
	return static_cast<resource::loader<T, context>*>(base_loader);
}

auto gse::gpu::context::loader(const id type_id) const -> resource::loader_base* {
	assert(m_resource_loaders.contains(type_id), std::source_location::current(), "Resource loader for id {} does not exist.", type_id.number());
	return m_resource_loaders.at(type_id).get();
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

auto gse::gpu::context::add_transient_work(auto&& commands) -> void {
	m_frame->add_transient_work(std::forward<decltype(commands)>(commands));
}

auto gse::gpu::context::descriptor_heap(this auto& self) -> decltype(auto) {
	return self.m_device->descriptor_heap();
}

auto gse::gpu::context::on_swap_chain_recreate(swap_chain_recreate_callback callback) const -> void {
	m_swapchain->on_recreate(std::move(callback));
}

auto gse::gpu::context::graph() const -> vulkan::render_graph& {
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
	for (auto& l : m_resource_loaders | std::views::values) {
		l.reset();
	}
	m_resource_loaders.clear();
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

auto gse::gpu::context::enumerate_resources(const std::string& baked_dir, const std::string& baked_ext) -> std::vector<std::string> {
	std::vector<std::string> result;

	const auto dir_path = config::baked_resource_path / baked_dir;

	if (!std::filesystem::exists(dir_path)) {
		return result;
	}

	for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		if (entry.path().extension().string() != baked_ext) {
			continue;
		}

		result.push_back(entry.path().stem().string());
	}

	std::ranges::sort(result);
	return result;
}
