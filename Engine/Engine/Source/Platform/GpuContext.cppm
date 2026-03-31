export module gse.platform:gpu_context;

import std;

import :resource_loader;
import :asset_pipeline;
import :asset_compiler;
import :shader_layout;
import :shader_layout_compiler;
import :window;
import :input_state;
import :render_graph;
import :gpu_device;
import :gpu_swapchain;
import :gpu_frame;

import gse.log;
import gse.utility;
import :vulkan_runtime;
import :vulkan_context;
import :vulkan_uploader;
import :descriptor_heap;

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

		template <typename Resource>
		auto queue_gpu_command(
			Resource* resource, 
			std::function<void(context&, Resource&)> work
		) const -> void;

		[[nodiscard]] auto execute_and_detect_gpu_queue(
			const std::function<void(const context&)>& work
		) const -> bool;

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
		) -> std::expected<gpu::frame_token, gpu::frame_status>;

		auto end_frame(
		) -> void;

		auto wait_idle(
		) -> void;

		[[nodiscard]] auto allocator(
		) -> vulkan::allocator&;

		[[nodiscard]] auto device(
		) -> vk::raii::Device&;

		[[nodiscard]] auto device(
		) const -> const vk::raii::Device&;

		[[nodiscard]] auto descriptor_heap(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto surface_format(
		) const -> vk::Format;

		[[nodiscard]] auto compute_queue_family(
		) const -> std::uint32_t;

		[[nodiscard]] auto compute_queue_ref(
		) -> const vk::raii::Queue&;

		[[nodiscard]] auto physical_device(
		) -> const vk::raii::PhysicalDevice&;

		auto add_transient_work(
			const auto& commands
		) -> void;

		using swap_chain_recreate_callback = std::function<void()>;
		auto on_swap_chain_recreate(
			swap_chain_recreate_callback callback
		) const -> void;

		auto upload_image_2d(
			vulkan::image_resource& resource,
			vec2u size,
			const void* pixel_data,
			std::size_t data_size,
			vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) -> void;

		auto upload_image_layers(
			vulkan::image_resource& resource,
			vec2u size,
			const std::vector<const void*>& face_data,
			std::size_t bytes_per_face,
			vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) -> void;

		[[nodiscard]] auto window(
		) -> window&;

		[[nodiscard]] auto window(
		) const -> const gse::window&;

		[[nodiscard]] auto gpu_queue_size(
		) const -> size_t;

		auto mark_pending_for_finalization(
			const std::type_index& resource_type, 
			id resource_id
		) const -> void;

		[[nodiscard]] auto graph(
		) const -> vulkan::render_graph&;

		auto set_ui_focus(
			bool focus
		) -> void;

		[[nodiscard]] auto ui_focus(
		) const -> bool;

		auto shutdown(
		) -> void;

		[[nodiscard]] auto device_ref(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto swapchain(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto frame(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto shader_layout(
			const std::string& name
		) const -> const gse::shader_layout*;

		auto load_layouts(
		) -> void;

		[[nodiscard]] static auto enumerate_resources(
			const std::string& baked_dir,
			const std::string& baked_ext
		) -> std::vector<std::string>;
	private:
		auto loader(
			const std::type_index& type_index
		) const -> resource::loader_base*;

		gse::window m_window;
		std::unique_ptr<vulkan::runtime> m_runtime;
		gpu::device m_device;
		gpu::swapchain m_swapchain;
		gpu::frame m_frame;
		asset_pipeline m_pipeline{ config::resource_path, config::baked_resource_path };
		std::unordered_map<std::type_index, std::unique_ptr<resource::loader_base>> m_resource_loaders;

		mutable std::vector<command> m_command_queue;
		mutable std::vector<std::pair<std::type_index, id>> m_pending_gpu_resources;
		mutable std::recursive_mutex m_mutex;

		std::unique_ptr<vulkan::render_graph> m_render_graph;
		bool m_ui_focus = false;
		bool m_validation_layers_enabled = false;

		std::unordered_map<std::string, std::unique_ptr<gse::shader_layout>> m_shader_layouts;
	};
}

gse::gpu::context::context(const std::string& window_title, input::system_state& input, save::state& save) : m_window(window_title, input, save), m_runtime(vulkan::create_runtime(m_window.raw_handle(), save)), m_device(*m_runtime), m_swapchain(*m_runtime), m_frame(*m_runtime), m_render_graph(std::make_unique<vulkan::render_graph>(m_device, m_swapchain, m_frame)) {
	save.bind("Graphics", "Validation Layers", m_validation_layers_enabled)
		.description("Enable Vulkan validation layers for debugging (impacts performance significantly)")
		.default_value(false)
		.restart_required()
		.commit();
}

gse::gpu::context::~context() {
	m_runtime.reset();
}

auto gse::gpu::context::device_ref(this auto& self) -> decltype(auto) {
	using self_t = std::remove_reference_t<decltype(self)>;
	if constexpr (std::is_const_v<self_t>) {
		return static_cast<const gpu::device&>(self.m_device);
	} else {
		return static_cast<gpu::device&>(self.m_device);
	}
}

auto gse::gpu::context::swapchain(this auto& self) -> decltype(auto) {
	using self_t = std::remove_reference_t<decltype(self)>;
	if constexpr (std::is_const_v<self_t>) {
		return static_cast<const gpu::swapchain&>(self.m_swapchain);
	} else {
		return static_cast<gpu::swapchain&>(self.m_swapchain);
	}
}

auto gse::gpu::context::frame(this auto& self) -> decltype(auto) {
	using self_t = std::remove_reference_t<decltype(self)>;
	if constexpr (std::is_const_v<self_t>) {
		return static_cast<const gpu::frame&>(self.m_frame);
	} else {
		return static_cast<gpu::frame&>(self.m_frame);
	}
}

template <typename T>
auto gse::gpu::context::add_loader() -> resource::loader<T, context>* {
	const auto type_index = std::type_index(typeid(T));
	assert(
		!m_resource_loaders.contains(type_index),
		std::source_location::current(),
		"Resource loader for type {} already exists.",
		type_index.name()
	);

	auto new_loader = std::make_unique<resource::loader<T, context>>(*this);
	auto* loader_ptr = new_loader.get();
	m_resource_loaders[type_index] = std::move(new_loader);

	if constexpr (has_asset_compiler<T>) {
		m_pipeline.register_type<T, context>(loader_ptr);
	}

	return loader_ptr;
}

template <typename T>
auto gse::gpu::context::get(id id) const -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->get(id);
}

template <typename T>
auto gse::gpu::context::get(const std::string& filename) const -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->get(filename);
}

template <typename T>
auto gse::gpu::context::try_get(id id) const -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->try_get(id);
}

template <typename T>
auto gse::gpu::context::try_get(const std::string& filename) const -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->try_get(filename);
}

template <typename T, typename... Args>
auto gse::gpu::context::queue(const std::string& name, Args&&... args) -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->queue(name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::gpu::context::queue_gpu_command(Resource* resource, std::function<void(context&, Resource&)> work) const -> void {
	command final_command = [resource, work_lambda = std::move(work)](context& ctx) {
		work_lambda(ctx, *resource);
	};

	std::lock_guard lock(m_mutex);
	m_command_queue.push_back(std::move(final_command));
}

template <typename T>
auto gse::gpu::context::instantly_load(const resource::handle<T>& handle) -> void {
	auto* specific_loader = loader<T>();
	specific_loader->instantly_load(handle.id());
}

template <typename T>
auto gse::gpu::context::add(T&& resource) -> resource::handle<T> {
	auto* specific_loader = loader<T>();
	return specific_loader->add(std::forward<T>(resource));
}

auto gse::gpu::context::execute_and_detect_gpu_queue(const std::function<void(const context&)>& work) const -> bool {
	std::lock_guard lock(m_mutex);
	const size_t queue_size_before = m_command_queue.size();

	work(*this);

	const size_t queue_size_after = m_command_queue.size();
	return queue_size_after > queue_size_before;
}

auto gse::gpu::context::process_resource_queue() -> void {
	for (const auto& loader : m_resource_loaders | std::views::values) {
		loader->flush();
	}
}

auto gse::gpu::context::process_gpu_queue() -> void {
	std::vector<command> commands_to_run;
	std::vector<std::pair<std::type_index, id>> resources_to_finalize;

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
		resource::loader_base* specific_loader = this->loader(type);
		specific_loader->update_state(resource_id, resource::state::loaded);
	}
}

auto gse::gpu::context::compile() -> void {
	m_pipeline.register_compiler_only<gse::shader_layout>();

	if (const auto result = m_pipeline.compile_all(); result.success_count > 0 || result.failure_count > 0) {
		log::println(
			result.failure_count > 0 ? log::level::warning : log::level::info,
			log::category::assets,
			"Compiled {} assets ({} skipped, {} failed)",
			result.success_count, result.skipped_count, result.failure_count
		);
	}

	load_layouts();
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
	for (const auto& loader : m_resource_loaders | std::views::values) {
		loader->finalize_reloads();
	}
}

template <typename T>
auto gse::gpu::context::resource_state(const id id) const -> resource::state {
	const auto type_index = std::type_index(typeid(T));
	const auto* loader = this->loader(type_index);
	return loader->resource_state(id);
}

template <typename T>
auto gse::gpu::context::loader(this auto&& self) -> decltype(auto) {
	const auto type_index = std::type_index(typeid(T));
	auto* base_loader = self.loader(type_index);
	return static_cast<resource::loader<T, context>*>(base_loader);
}

auto gse::gpu::context::begin_frame() -> std::expected<gpu::frame_token, gpu::frame_status> {
	auto result = vulkan::begin_frame({
		.window = m_window.raw_handle(),
		.frame_buffer_resized = m_window.frame_buffer_resized(),
		.minimized = m_window.minimized(),
		.runtime = *m_runtime
	});

	if (result) {
		m_device.descriptor_heap().begin_frame(result->frame_index);
	}

	return result;
}

auto gse::gpu::context::end_frame() -> void {
	vulkan::end_frame({
		.window = m_window.raw_handle(),
		.frame_buffer_resized = m_window.frame_buffer_resized(),
		.minimized = m_window.minimized(),
		.runtime = *m_runtime
	});
}

auto gse::gpu::context::wait_idle() -> void {
	m_device.wait_idle();
}

auto gse::gpu::context::allocator() -> vulkan::allocator& {
	return m_device.runtime_ref().allocator();
}

auto gse::gpu::context::device() -> vk::raii::Device& {
	return m_device.logical_device();
}

auto gse::gpu::context::device() const -> const vk::raii::Device& {
	return m_device.logical_device();
}

auto gse::gpu::context::add_transient_work(const auto& commands) -> void {
	m_device.add_transient_work(commands);
}

auto gse::gpu::context::descriptor_heap(this auto& self) -> decltype(auto) {
	return self.m_device.descriptor_heap();
}

auto gse::gpu::context::surface_format() const -> vk::Format {
	return m_swapchain.format();
}

auto gse::gpu::context::compute_queue_family() const -> std::uint32_t {
	return m_device.compute_queue_family();
}

auto gse::gpu::context::compute_queue_ref() -> const vk::raii::Queue& {
	return m_device.compute_queue();
}

auto gse::gpu::context::physical_device() -> const vk::raii::PhysicalDevice& {
	return m_device.physical_device();
}

auto gse::gpu::context::on_swap_chain_recreate(swap_chain_recreate_callback callback) const -> void {
	m_runtime->on_swap_chain_recreate(std::move(callback));
}

auto gse::gpu::context::upload_image_2d(vulkan::image_resource& resource, const vec2u size, const void* pixel_data, const std::size_t data_size, const vk::ImageLayout final_layout) -> void {
	vulkan::uploader::upload_image_2d(*m_runtime, resource, size, pixel_data, data_size, final_layout);
}

auto gse::gpu::context::upload_image_layers(vulkan::image_resource& resource, const vec2u size, const std::vector<const void*>& face_data, const std::size_t bytes_per_face, const vk::ImageLayout final_layout) -> void {
	vulkan::uploader::upload_image_layers(*m_runtime, resource, size, face_data, bytes_per_face, final_layout);
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

auto gse::gpu::context::gpu_queue_size() const -> size_t {
	std::lock_guard lock(m_mutex);
	return m_command_queue.size();
}

auto gse::gpu::context::mark_pending_for_finalization(const std::type_index& resource_type, id resource_id) const -> void {
	std::lock_guard lock(m_mutex);
	m_pending_gpu_resources.emplace_back(resource_type, resource_id);
}

auto gse::gpu::context::set_ui_focus(const bool focus) -> void {
	m_ui_focus = focus;
}

auto gse::gpu::context::ui_focus() const -> bool {
	return m_ui_focus;
}

auto gse::gpu::context::shutdown() -> void {
	if (!m_runtime) return;

	m_window.shutdown();

	m_device.wait_idle();

	for (auto& loader : m_resource_loaders | std::views::values) {
		loader.reset();
	}

	m_resource_loaders.clear();
	m_shader_layouts.clear();

	m_runtime->swap_chain_config().depth_image = {};
}

auto gse::gpu::context::loader(const std::type_index& type_index) const -> resource::loader_base* {
	assert(m_resource_loaders.contains(type_index), std::source_location::current(), "Resource loader for type {} does not exist.", type_index.name());
	return m_resource_loaders.at(type_index).get();
}

auto gse::gpu::context::shader_layout(const std::string& name) const -> const gse::shader_layout* {
	if (const auto it = m_shader_layouts.find(name); it != m_shader_layouts.end()) {
		return it->second.get();
	}
	return nullptr;
}

auto gse::gpu::context::load_layouts() -> void {
	const auto layouts_dir = config::baked_resource_path / "Layouts";
	if (!std::filesystem::exists(layouts_dir)) {
		return;
	}

	for (const auto& entry : std::filesystem::directory_iterator(layouts_dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".glayout") {
			continue;
		}

		auto layout = std::make_unique<gse::shader_layout>(entry.path());
		layout->load(m_device.logical_device());

		const auto& name = layout->name();
		log::println(log::category::assets, "Layout loaded: {}", name);
		m_shader_layouts[name] = std::move(layout);
	}
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
