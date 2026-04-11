export module gse.platform:gpu_context;

import std;

import :gpu_resource_manager;
import :window;
import :input_state;
import :render_graph;
import :gpu_device;
import :gpu_swapchain;
import :gpu_frame;
import :frame_scheduler;

import gse.utility;

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

		template <typename T>
		auto add_loader(
		) -> resource::loader<T, resource_manager>*;

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
			std::function<void(resource_manager&, Resource&)> work
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

		auto wait_idle(
		) -> void;

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

		[[nodiscard]] auto device_ref(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto swapchain(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto frame(
			this auto& self
		) -> auto&;

		[[nodiscard]] static auto enumerate_resources(
			const std::string& baked_dir,
			const std::string& baked_ext
		) -> std::vector<std::string>;

	private:
		gse::window m_window;
		std::unique_ptr<device> m_device;
		std::unique_ptr<swap_chain> m_swapchain;
		std::unique_ptr<gpu::frame> m_frame;
		resource_manager m_resource_manager;
		std::unique_ptr<vulkan::render_graph> m_render_graph;
		frame_scheduler m_scheduler;
		bool m_ui_focus = false;
		bool m_validation_layers_enabled = false;
	};
}

gse::gpu::context::context(const std::string& window_title, input::system_state& input, save::state& save)
	: m_window(window_title, input, save)
	, m_device(device::create(m_window, save))
	, m_swapchain(swap_chain::create(m_window.viewport(), *m_device))
	, m_frame(frame::create(*m_device, *m_swapchain))
	, m_resource_manager(*m_device)
{
	m_render_graph = std::make_unique<vulkan::render_graph>(*m_device, *m_swapchain, *m_frame);

	save.bind("Graphics", "Validation Layers", m_validation_layers_enabled)
		.description("Enable Vulkan validation layers for debugging (impacts performance significantly)")
		.default_value(false)
		.restart_required()
		.commit();

	auto transition_depth = [this]() {
		m_device->transition_image_layout(m_render_graph->depth_image(), image_layout::general);
	};
	transition_depth();
	m_swapchain->on_recreate([transition_depth]() { transition_depth(); });
}

gse::gpu::context::~context() {
	m_frame.reset();
	m_swapchain.reset();
	m_device.reset();
}

template <typename T>
auto gse::gpu::context::add_loader() -> resource::loader<T, resource_manager>* {
	return m_resource_manager.add_loader<T>();
}

template <typename T>
auto gse::gpu::context::get(id id) const -> resource::handle<T> {
	return m_resource_manager.get<T>(id);
}

template <typename T>
auto gse::gpu::context::get(const std::string& filename) const -> resource::handle<T> {
	return m_resource_manager.get<T>(filename);
}

template <typename T>
auto gse::gpu::context::try_get(id id) const -> resource::handle<T> {
	return m_resource_manager.try_get<T>(id);
}

template <typename T>
auto gse::gpu::context::try_get(const std::string& filename) const -> resource::handle<T> {
	return m_resource_manager.try_get<T>(filename);
}

template <typename T, typename... Args>
auto gse::gpu::context::queue(const std::string& name, Args&&... args) -> resource::handle<T> {
	return m_resource_manager.queue<T>(name, std::forward<Args>(args)...);
}

template <typename Resource>
auto gse::gpu::context::queue_gpu_command(Resource* resource, std::function<void(resource_manager&, Resource&)> work) const -> void {
	m_resource_manager.queue_gpu_command(resource, std::move(work));
}

template <typename T>
auto gse::gpu::context::instantly_load(const resource::handle<T>& handle) -> void {
	m_resource_manager.instantly_load(handle);
}

template <typename T>
auto gse::gpu::context::add(T&& resource) -> resource::handle<T> {
	return m_resource_manager.add(std::forward<T>(resource));
}

auto gse::gpu::context::process_resource_queue() -> void {
	m_resource_manager.process_resource_queue();
}

auto gse::gpu::context::process_gpu_queue() -> void {
	m_resource_manager.process_gpu_queue();
}

auto gse::gpu::context::compile() -> void {
	m_resource_manager.compile();
}

auto gse::gpu::context::poll_assets() -> void {
	m_resource_manager.poll_assets();
}

auto gse::gpu::context::enable_hot_reload() -> void {
	m_resource_manager.enable_hot_reload();
}

auto gse::gpu::context::disable_hot_reload() -> void {
	m_resource_manager.disable_hot_reload();
}

auto gse::gpu::context::hot_reload_enabled() const -> bool {
	return m_resource_manager.hot_reload_enabled();
}

auto gse::gpu::context::finalize_reloads() -> void {
	m_resource_manager.finalize_reloads();
}

template <typename T>
auto gse::gpu::context::resource_state(const id id) const -> resource::state {
	return m_resource_manager.resource_state<T>(id);
}

template <typename T>
auto gse::gpu::context::loader(this auto&& self) -> decltype(auto) {
	return self.m_resource_manager.template loader<T>();
}

auto gse::gpu::context::begin_frame() -> std::expected<frame_token, frame_status> {
	auto result = m_frame->begin(m_window);

	if (result) {
		m_device->descriptor_heap().begin_frame(result->frame_index);
	}

	return result;
}

auto gse::gpu::context::end_frame() -> void {
	m_frame->end(m_window);
}

auto gse::gpu::context::wait_idle() -> void {
	m_device->wait_idle();
}

auto gse::gpu::context::add_transient_work(auto&& commands) -> void {
	m_device->add_transient_work(std::forward<decltype(commands)>(commands));
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
	m_resource_manager.shutdown();
	m_swapchain->clear_depth_image();
}

auto gse::gpu::context::device_ref(this auto& self) -> auto& {
	return *self.m_device;
}

auto gse::gpu::context::swapchain(this auto& self) -> auto& {
	return *self.m_swapchain;
}

auto gse::gpu::context::frame(this auto& self) -> auto& {
	return *self.m_frame;
}

auto gse::gpu::context::enumerate_resources(const std::string& baked_dir, const std::string& baked_ext) -> std::vector<std::string> {
	return resource_manager::enumerate_resources(baked_dir, baked_ext);
}
