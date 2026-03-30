export module gse.platform:render_graph;

import std;

import :gpu_types;

import gse.assert;
import :vulkan_config;
import gse.utility;
import gse.math;

export namespace gse::vulkan {
	class render_graph;
	class pass_builder;

	struct push_constant_member {
		std::uint32_t offset = 0;
		std::uint32_t size = 0;
	};

	struct cached_push_constants {
		std::vector<std::byte> data;
		vk::ShaderStageFlags stage_flags{};
		std::unordered_map<std::string, push_constant_member> members;

		template <typename T>
		auto set(
			std::string_view name,
			const T& value
		) -> void;

		auto replay(
			vk::CommandBuffer cmd,
			vk::PipelineLayout layout
		) const -> void;
	};

	struct color_clear {
		float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	};

	struct depth_clear {
		float depth = 1.0f;
	};

	enum class load_op {
		clear_color,
		clear_depth,
		load,
		dont_care
	};

	struct color_output_info {
		bool is_swapchain = false;
		const image_resource* custom_target = nullptr;
		load_op op = load_op::clear_color;
		color_clear clear_value;
	};

	struct depth_output_info {
		load_op op = load_op::clear_depth;
		depth_clear clear_value;
	};


	enum class resource_type : std::uint8_t {
		buffer,
		image
	};

	struct resource_ref {
		const void* ptr = nullptr;
		resource_type type = resource_type::buffer;
	};

	struct resource_usage {
		resource_ref resource;
		vk::PipelineStageFlags2 stage;
		vk::AccessFlags2 access;
	};

	auto sampled(
		const image_resource& img,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto storage(
		const buffer_resource& buf,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto storage_read(
		const buffer_resource& buf,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto indirect_read(
		const buffer_resource& buf,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto attachment(
		const image_resource& img,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	class recording_context {
	public:
		auto bind_pipeline(
			vk::PipelineBindPoint point,
			vk::Pipeline pipeline
		) -> void;

		auto bind_descriptor_sets(
			vk::PipelineBindPoint point,
			vk::PipelineLayout layout,
			std::uint32_t first_set,
			std::span<const vk::DescriptorSet> sets,
			std::span<const std::uint32_t> offsets = {}
		) -> void;

		auto push_descriptor(
			vk::PipelineBindPoint point,
			vk::PipelineLayout layout,
			std::uint32_t set_index,
			std::span<const vk::WriteDescriptorSet> writes
		) -> void;

		auto push(
			const cached_push_constants& cache,
			vk::PipelineLayout layout
		) -> void;

		auto set_viewport(
			float x,
			float y,
			float width,
			float height,
			float min_depth = 0.0f,
			float max_depth = 1.0f
		) -> void;

		auto set_scissor(
			std::int32_t x,
			std::int32_t y,
			std::uint32_t width,
			std::uint32_t height
		) -> void;

		auto draw(
			std::uint32_t vertex_count,
			std::uint32_t instance_count = 1,
			std::uint32_t first_vertex = 0,
			std::uint32_t first_instance = 0
		) -> void;

		auto draw_indexed(
			std::uint32_t index_count,
			std::uint32_t instance_count = 1,
			std::uint32_t first_index = 0,
			std::int32_t vertex_offset = 0,
			std::uint32_t first_instance = 0
		) -> void;

		auto draw_indirect(
			vk::Buffer buffer,
			vk::DeviceSize offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) -> void;

		auto draw_mesh_tasks(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
		) -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
		) -> void;

		auto bind_vertex_buffers(
			std::uint32_t first,
			std::span<const vk::Buffer> buffers,
			std::span<const vk::DeviceSize> offsets
		) -> void;

		auto bind_index_buffer(
			vk::Buffer buffer,
			vk::DeviceSize offset,
			vk::IndexType index_type
		) -> void;

		auto begin_rendering(
			const vk::RenderingInfo& info
		) -> void;

		auto end_rendering(
		) -> void;

		auto bind_pipeline(
			vk::PipelineBindPoint point,
			const gpu::pipeline& p
		) -> void;

		auto bind_descriptor_set(
			vk::PipelineBindPoint point,
			const gpu::pipeline& p,
			std::uint32_t first_set,
			const gpu::descriptor_set& set
		) -> void;

		auto push(
			const gpu::pipeline& p,
			const cached_push_constants& cache
		) -> void;

		auto draw_indirect(
			const gpu::buffer& buf,
			vk::DeviceSize offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) -> void;

		auto push_storage_buffers(
			vk::PipelineBindPoint point,
			const gpu::pipeline& p,
			std::uint32_t set_index,
			std::span<const gpu::descriptor_buffer_binding> bindings
		) -> void;

		auto bind_vertex_buffer(
			const gpu::buffer& buf,
			vk::DeviceSize offset = 0
		) -> void;

		auto bind_index_buffer(
			const gpu::buffer& buf,
			vk::DeviceSize offset = 0,
			vk::IndexType index_type = vk::IndexType::eUint32
		) -> void;

	private:
		friend class render_graph;
		vk::CommandBuffer m_cmd;
		explicit recording_context(vk::CommandBuffer cmd);
	};

	struct render_pass_data {
		std::type_index pass_type = typeid(void);
		std::vector<resource_usage> reads;
		std::vector<resource_usage> writes;
		std::vector<const buffer_resource*> tracked_buffers;
		vk::PipelineStageFlags2 tracked_stage = vk::PipelineStageFlagBits2::eAllCommands;
		std::vector<std::type_index> after_passes;
		bool enabled = true;
		std::optional<color_output_info> color_output;
		std::optional<depth_output_info> depth_output;
		std::move_only_function<void(recording_context&)> record_fn;
	};

	class pass_builder {
	public:
		pass_builder(pass_builder&&) = default;
		auto operator=(pass_builder&&) -> pass_builder& = default;
		~pass_builder();

		auto track(
			const buffer_resource& buf
		) -> pass_builder&;

		auto track(
			const gpu::buffer& buf
		) -> pass_builder&;

		template <typename... Args>
		auto reads(
			Args&&... args
		) -> pass_builder&;

		template <typename... Args>
		auto writes(
			Args&&... args
		) -> pass_builder&;

		template <typename T>
		auto after(
		) -> pass_builder&;

		auto when(
			bool condition
		) -> pass_builder&;

		auto color_output(
			const color_clear& clear_value
		) -> pass_builder&;

		auto color_output_load(
		) -> pass_builder&;

		auto depth_output(
			const depth_clear& clear_value
		) -> pass_builder&;

		auto depth_output_load(
		) -> pass_builder&;

		auto record(
			std::move_only_function<void(recording_context&)> fn
		) -> void;

	private:
		friend class render_graph;
		friend struct gpu_buffer_handle;
		explicit pass_builder(
			render_graph& graph,
			std::type_index pass_type
		);

		auto submit(
		) -> void;

		auto add_tracked(
			const buffer_resource* buf
		) -> void;

		render_graph* m_graph;
		render_pass_data m_pass;
		bool m_submitted = false;
	};

	class render_graph {
	public:
		explicit render_graph(
			config& cfg
		);

		template <typename PassType>
		[[nodiscard]] auto add_pass(
		) -> pass_builder;

		auto execute(
		) -> void;

		auto clear(
		) -> void;

		[[nodiscard]] auto current_frame(
		) const -> std::uint32_t;

		[[nodiscard]] auto extent(
		) const -> vec2u;

		[[nodiscard]] auto swapchain_image_view(
		) const -> vk::ImageView;

		[[nodiscard]] auto depth_image_view(
		) const -> vk::ImageView;

		[[nodiscard]] auto depth_image(
		) const -> const image_resource&;

		[[nodiscard]] auto frame_in_progress(
		) const -> bool;

	private:
		friend class pass_builder;

		auto submit_pass(
			render_pass_data pass
		) -> void;

		config* m_config;
		std::vector<render_pass_data> m_passes;
	};
}

template <typename T>
auto gse::vulkan::cached_push_constants::set(const std::string_view name, const T& value) -> void {
	const auto it = members.find(std::string(name));
	assert(it != members.end(), std::source_location::current(), "Push constant member '{}' not found", name);
	assert(sizeof(T) <= it->second.size, std::source_location::current(), "Push constant member '{}' size mismatch", name);
	std::memcpy(data.data() + it->second.offset, std::addressof(value), sizeof(T));
}

auto gse::vulkan::cached_push_constants::replay(vk::CommandBuffer cmd, vk::PipelineLayout layout) const -> void {
	cmd.pushConstants(layout, stage_flags, 0, static_cast<std::uint32_t>(data.size()), data.data());
}


gse::vulkan::recording_context::recording_context(vk::CommandBuffer cmd) : m_cmd(cmd) {}

auto gse::vulkan::recording_context::bind_pipeline(vk::PipelineBindPoint point, vk::Pipeline pipeline) -> void {
	m_cmd.bindPipeline(point, pipeline);
}

auto gse::vulkan::recording_context::bind_descriptor_sets(vk::PipelineBindPoint point, vk::PipelineLayout layout, std::uint32_t first_set, std::span<const vk::DescriptorSet> sets, std::span<const std::uint32_t> offsets) -> void {
	m_cmd.bindDescriptorSets(point, layout, first_set, static_cast<std::uint32_t>(sets.size()), sets.data(), static_cast<std::uint32_t>(offsets.size()), offsets.data());
}

auto gse::vulkan::recording_context::push_descriptor(vk::PipelineBindPoint point, vk::PipelineLayout layout, std::uint32_t set_index, std::span<const vk::WriteDescriptorSet> writes) -> void {
	m_cmd.pushDescriptorSetKHR(point, layout, set_index, static_cast<std::uint32_t>(writes.size()), writes.data());
}

auto gse::vulkan::recording_context::push(const cached_push_constants& cache, vk::PipelineLayout layout) -> void {
	cache.replay(m_cmd, layout);
}

auto gse::vulkan::recording_context::set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) -> void {
	const vk::Viewport vp{ .x = x, .y = y, .width = width, .height = height, .minDepth = min_depth, .maxDepth = max_depth };
	m_cmd.setViewport(0, vp);
}

auto gse::vulkan::recording_context::set_scissor(std::int32_t x, std::int32_t y, std::uint32_t width, std::uint32_t height) -> void {
	const vk::Rect2D sc{ .offset = { x, y }, .extent = { width, height } };
	m_cmd.setScissor(0, sc);
}

auto gse::vulkan::recording_context::draw(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance) -> void {
	m_cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::vulkan::recording_context::draw_indexed(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) -> void {
	m_cmd.drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::vulkan::recording_context::draw_indirect(vk::Buffer buffer, vk::DeviceSize offset, std::uint32_t draw_count, std::uint32_t stride) -> void {
	m_cmd.drawIndexedIndirect(buffer, offset, draw_count, stride);
}

auto gse::vulkan::recording_context::draw_mesh_tasks(std::uint32_t x, std::uint32_t y, std::uint32_t z) -> void {
	m_cmd.drawMeshTasksEXT(x, y, z);
}

auto gse::vulkan::recording_context::dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z) -> void {
	m_cmd.dispatch(x, y, z);
}

auto gse::vulkan::recording_context::bind_vertex_buffers(std::uint32_t first, std::span<const vk::Buffer> buffers, std::span<const vk::DeviceSize> offsets) -> void {
	m_cmd.bindVertexBuffers(first, static_cast<std::uint32_t>(buffers.size()), buffers.data(), offsets.data());
}

auto gse::vulkan::recording_context::bind_index_buffer(vk::Buffer buffer, vk::DeviceSize offset, vk::IndexType index_type) -> void {
	m_cmd.bindIndexBuffer(buffer, offset, index_type);
}

auto gse::vulkan::recording_context::begin_rendering(const vk::RenderingInfo& info) -> void {
	m_cmd.beginRendering(info);
}

auto gse::vulkan::recording_context::end_rendering() -> void {
	m_cmd.endRendering();
}

auto gse::vulkan::recording_context::bind_pipeline(const vk::PipelineBindPoint point, const gpu::pipeline& p) -> void {
	m_cmd.bindPipeline(point, p.native_pipeline());
}

auto gse::vulkan::recording_context::bind_descriptor_set(const vk::PipelineBindPoint point, const gpu::pipeline& p, const std::uint32_t first_set, const gpu::descriptor_set& set) -> void {
	const vk::DescriptorSet native_set = set.native();
	m_cmd.bindDescriptorSets(point, p.native_layout(), first_set, 1, &native_set, 0, nullptr);
}

auto gse::vulkan::recording_context::push(const gpu::pipeline& p, const cached_push_constants& cache) -> void {
	cache.replay(m_cmd, p.native_layout());
}

auto gse::vulkan::recording_context::draw_indirect(const gpu::buffer& buf, const vk::DeviceSize offset, const std::uint32_t draw_count, const std::uint32_t stride) -> void {
	m_cmd.drawIndexedIndirect(buf.native().buffer, offset, draw_count, stride);
}

auto gse::vulkan::recording_context::push_storage_buffers(const vk::PipelineBindPoint point, const gpu::pipeline& p, const std::uint32_t set_index, const std::span<const gpu::descriptor_buffer_binding> bindings) -> void {
	std::vector<vk::DescriptorBufferInfo> buffer_infos;
	std::vector<vk::WriteDescriptorSet> writes;
	buffer_infos.reserve(bindings.size());
	writes.reserve(bindings.size());

	for (const auto& b : bindings) {
		buffer_infos.push_back({
			.buffer = b.buf->native().buffer,
			.offset = b.offset,
			.range = b.range == 0 ? b.buf->size() : b.range
		});
	}

	for (std::size_t i = 0; i < bindings.size(); ++i) {
		writes.push_back({
			.dstBinding = bindings[i].binding,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &buffer_infos[i]
		});
	}

	m_cmd.pushDescriptorSetKHR(point, p.native_layout(), set_index, static_cast<std::uint32_t>(writes.size()), writes.data());
}

auto gse::vulkan::recording_context::bind_vertex_buffer(const gpu::buffer& buf, const vk::DeviceSize offset) -> void {
	const vk::Buffer buffers[]{ buf.native().buffer };
	const vk::DeviceSize offsets[]{ offset };
	m_cmd.bindVertexBuffers(0, 1, buffers, offsets);
}

auto gse::vulkan::recording_context::bind_index_buffer(const gpu::buffer& buf, const vk::DeviceSize offset, const vk::IndexType index_type) -> void {
	m_cmd.bindIndexBuffer(buf.native().buffer, offset, index_type);
}

auto gse::vulkan::sampled(const image_resource& img, vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(img), .type = resource_type::image }, stage, vk::AccessFlagBits2::eShaderSampledRead };
}

auto gse::vulkan::storage(const buffer_resource& buf, vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite };
}

auto gse::vulkan::storage_read(const buffer_resource& buf, vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eShaderStorageRead };
}

auto gse::vulkan::indirect_read(const buffer_resource& buf, vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eIndirectCommandRead };
}

auto gse::vulkan::attachment(const image_resource& img, vk::PipelineStageFlags2 stage) -> resource_usage {
	const auto access = (stage == vk::PipelineStageFlagBits2::eLateFragmentTests || stage == vk::PipelineStageFlagBits2::eEarlyFragmentTests)
		? vk::AccessFlagBits2::eDepthStencilAttachmentWrite
		: vk::AccessFlagBits2::eColorAttachmentWrite;
	return { { .ptr = std::addressof(img), .type = resource_type::image }, stage, access };
}

namespace {
	const char swapchain_sentinel = 0;
}

gse::vulkan::pass_builder::pass_builder(render_graph& graph, std::type_index pass_type)
	: m_graph(std::addressof(graph)), m_pass{ .pass_type = pass_type } {}

gse::vulkan::pass_builder::~pass_builder() {
	assert(m_submitted, std::source_location::current(), "pass_builder destroyed without calling record()");
}

auto gse::vulkan::pass_builder::track(const buffer_resource& buf) -> pass_builder& {
	add_tracked(std::addressof(buf));
	return *this;
}

auto gse::vulkan::pass_builder::track(const gpu::buffer& buf) -> pass_builder& {
	add_tracked(std::addressof(buf.native()));
	return *this;
}

auto gse::vulkan::pass_builder::add_tracked(const buffer_resource* buf) -> void {
	for (const auto* existing : m_pass.tracked_buffers) {
		if (existing == buf) return;
	}
	m_pass.tracked_buffers.push_back(buf);
}

template <typename... Args>
auto gse::vulkan::pass_builder::reads(Args&&... args) -> pass_builder& {
	(m_pass.reads.push_back(std::forward<Args>(args)), ...);
	return *this;
}

template <typename... Args>
auto gse::vulkan::pass_builder::writes(Args&&... args) -> pass_builder& {
	(m_pass.writes.push_back(std::forward<Args>(args)), ...);
	return *this;
}

template <typename T>
auto gse::vulkan::pass_builder::after() -> pass_builder& {
	m_pass.after_passes.push_back(std::type_index(typeid(T)));
	return *this;
}

auto gse::vulkan::pass_builder::when(const bool condition) -> pass_builder& {
	m_pass.enabled = condition;
	return *this;
}

auto gse::vulkan::pass_builder::color_output(const color_clear& clear_value) -> pass_builder& {
	m_pass.color_output = color_output_info{
		.is_swapchain = true,
		.op = load_op::clear_color,
		.clear_value = clear_value
	};
	return *this;
}

auto gse::vulkan::pass_builder::color_output_load() -> pass_builder& {
	m_pass.color_output = color_output_info{
		.is_swapchain = true,
		.op = load_op::load
	};
	m_pass.reads.push_back({
		{ .ptr = &swapchain_sentinel, .type = resource_type::image },
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentRead
	});
	return *this;
}

auto gse::vulkan::pass_builder::depth_output(const depth_clear& clear_value) -> pass_builder& {
	m_pass.depth_output = depth_output_info{
		.op = load_op::clear_depth,
		.clear_value = clear_value
	};
	m_pass.writes.push_back(attachment(m_graph->m_config->swap_chain_config().depth_image, vk::PipelineStageFlagBits2::eLateFragmentTests));
	return *this;
}

auto gse::vulkan::pass_builder::depth_output_load() -> pass_builder& {
	m_pass.depth_output = depth_output_info{
		.op = load_op::load
	};
	return *this;
}

auto gse::vulkan::pass_builder::record(std::move_only_function<void(recording_context&)> fn) -> void {
	if (m_pass.color_output) {
		m_pass.writes.push_back({
			{ .ptr = &swapchain_sentinel, .type = resource_type::image },
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
		});
	}

	m_pass.record_fn = std::move(fn);
	m_submitted = true;
	submit();
}

auto gse::vulkan::pass_builder::submit() -> void {
	m_graph->submit_pass(std::move(m_pass));
}

gse::vulkan::render_graph::render_graph(config& cfg) : m_config(std::addressof(cfg)) {}

template <typename PassType>
auto gse::vulkan::render_graph::add_pass() -> pass_builder {
	return pass_builder(*this, std::type_index(typeid(PassType)));
}

auto gse::vulkan::render_graph::submit_pass(render_pass_data pass) -> void {
	if (pass.enabled) {
		m_passes.push_back(std::move(pass));
	}
}

auto gse::vulkan::render_graph::clear() -> void {
	m_passes.clear();
}

auto gse::vulkan::render_graph::current_frame() const -> std::uint32_t {
	return m_config->current_frame();
}

auto gse::vulkan::render_graph::extent() const -> vec2u {
	const auto ext = m_config->swap_chain_config().extent;
	return { ext.width, ext.height };
}

auto gse::vulkan::render_graph::swapchain_image_view() const -> vk::ImageView {
	return *m_config->swap_chain_config().image_views[m_config->frame_context().image_index];
}

auto gse::vulkan::render_graph::depth_image_view() const -> vk::ImageView {
	return m_config->swap_chain_config().depth_image.view;
}

auto gse::vulkan::render_graph::depth_image() const -> const image_resource& {
	return m_config->swap_chain_config().depth_image;
}

auto gse::vulkan::render_graph::frame_in_progress() const -> bool {
	return m_config->frame_in_progress();
}

auto gse::vulkan::render_graph::execute() -> void {
	if (!m_config->frame_in_progress()) {
		return;
	}

	const auto command = m_config->frame_context().command_buffer;
	const auto& swap = m_config->swap_chain_config();
	const auto image_index = m_config->frame_context().image_index;

	const vk::ImageMemoryBarrier2 begin_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
		.srcAccessMask = {},
		.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = swap.images[image_index],
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	const vk::DependencyInfo begin_dep{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &begin_barrier
	};
	command.pipelineBarrier2(begin_dep);

	if (m_passes.empty()) {
		const vk::ImageMemoryBarrier2 present_barrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
			.dstAccessMask = {},
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::ePresentSrcKHR,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = swap.images[image_index],
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		const vk::DependencyInfo end_dep{
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &present_barrier
		};
		command.pipelineBarrier2(end_dep);
		return;
	}

	std::unordered_map<std::type_index, std::size_t> type_to_index;
	for (std::size_t i = 0; i < m_passes.size(); ++i) {
		type_to_index[m_passes[i].pass_type] = i;
	}

	std::vector<std::vector<std::size_t>> adj(m_passes.size());
	std::vector<std::size_t> in_degree(m_passes.size(), 0);

	auto add_edge = [&](std::size_t from, std::size_t to) {
		for (const auto n : adj[from]) {
			if (n == to) return;
		}
		adj[from].push_back(to);
		++in_degree[to];
	};

	for (std::size_t i = 0; i < m_passes.size(); ++i) {
		for (const auto& dep : m_passes[i].after_passes) {
			if (auto it = type_to_index.find(dep); it != type_to_index.end()) {
				add_edge(it->second, i);
			}
		}
	}

	for (std::size_t i = 0; i < m_passes.size(); ++i) {
		for (std::size_t j = i + 1; j < m_passes.size(); ++j) {
			bool i_writes_j_reads = false;
			bool j_writes_i_reads = false;

			for (const auto& w : m_passes[i].writes) {
				for (const auto& r : m_passes[j].reads) {
					if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) i_writes_j_reads = true;
				}
			}
			for (const auto& w : m_passes[j].writes) {
				for (const auto& r : m_passes[i].reads) {
					if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) j_writes_i_reads = true;
				}
			}

			if (i_writes_j_reads && j_writes_i_reads) {
				add_edge(i, j);
			} else if (i_writes_j_reads) {
				add_edge(i, j);
			} else if (j_writes_i_reads) {
				add_edge(j, i);
			}
		}
	}

	std::vector<std::size_t> sorted;
	sorted.reserve(m_passes.size());

	std::queue<std::size_t> queue;
	for (std::size_t i = 0; i < m_passes.size(); ++i) {
		if (in_degree[i] == 0) queue.push(i);
	}

	while (!queue.empty()) {
		auto front = queue.front();
		queue.pop();
		sorted.push_back(front);
		for (auto next : adj[front]) {
			if (--in_degree[next] == 0) queue.push(next);
		}
	}

	recording_context ctx(command);

	for (std::size_t si = 0; si < sorted.size(); ++si) {
		auto& pass = m_passes[sorted[si]];

		std::vector<vk::MemoryBarrier2> barriers;

		if (!pass.tracked_buffers.empty()) {
			barriers.push_back({
				.srcStageMask = vk::PipelineStageFlagBits2::eHost,
				.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
				.dstStageMask = pass.tracked_stage,
				.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eUniformRead
			});
		}

		for (std::size_t pi = 0; pi < si; ++pi) {
			const auto& prev = m_passes[sorted[pi]];
			for (const auto& [prev_resource, prev_stage, prev_access] : prev.writes) {
				if (!prev_resource.ptr) continue;
				for (const auto& [read_resource, read_stage, read_access] : pass.reads) {
					if (read_resource.ptr && prev_resource.ptr == read_resource.ptr) {
						barriers.push_back({
							.srcStageMask = prev_stage,
							.srcAccessMask = prev_access,
							.dstStageMask = read_stage,
							.dstAccessMask = read_access
						});
					}
				}
				for (const auto& [cur_resource, cur_stage, cur_access] : pass.writes) {
					if (cur_resource.ptr && prev_resource.ptr == cur_resource.ptr) {
						barriers.push_back({
							.srcStageMask = prev_stage,
							.srcAccessMask = prev_access,
							.dstStageMask = cur_stage,
							.dstAccessMask = cur_access
						});
					}
				}
			}
		}

		if (!barriers.empty()) {
			const vk::DependencyInfo dep{
				.memoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
				.pMemoryBarriers = barriers.data()
			};
			command.pipelineBarrier2(dep);
		}

		bool has_graphics = pass.color_output || pass.depth_output;

		if (has_graphics) {
			std::vector<vk::RenderingAttachmentInfo> color_attachments;
			std::optional<vk::RenderingAttachmentInfo> depth_att;

			if (pass.color_output) {
				const auto& co = *pass.color_output;
				vk::AttachmentLoadOp vk_load = vk::AttachmentLoadOp::eDontCare;
				vk::ClearValue clear_val{};

				if (co.op == load_op::clear_color) {
					vk_load = vk::AttachmentLoadOp::eClear;
					clear_val.color = vk::ClearColorValue{
						.float32 = std::array{ co.clear_value.r, co.clear_value.g, co.clear_value.b, co.clear_value.a }
					};
				} else if (co.op == load_op::load) {
					vk_load = vk::AttachmentLoadOp::eLoad;
				}

				color_attachments.push_back({
					.imageView = *swap.image_views[image_index],
					.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.loadOp = vk_load,
					.storeOp = vk::AttachmentStoreOp::eStore,
					.clearValue = clear_val
				});
			}

			if (pass.depth_output) {
				const auto& dpo = *pass.depth_output;
				vk::AttachmentLoadOp vk_load = vk::AttachmentLoadOp::eDontCare;
				vk::ClearValue clear_val{};

				if (dpo.op == load_op::clear_depth) {
					vk_load = vk::AttachmentLoadOp::eClear;
					clear_val.depthStencil = vk::ClearDepthStencilValue{ .depth = dpo.clear_value.depth };
				} else if (dpo.op == load_op::load) {
					vk_load = vk::AttachmentLoadOp::eLoad;
				}

				depth_att = vk::RenderingAttachmentInfo{
					.imageView = swap.depth_image.view,
					.imageLayout = vk::ImageLayout::eGeneral,
					.loadOp = vk_load,
					.storeOp = vk::AttachmentStoreOp::eStore,
					.clearValue = clear_val
				};
			}

			const vk::RenderingInfo ri{
				.renderArea = { { 0, 0 }, swap.extent },
				.layerCount = 1,
				.colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size()),
				.pColorAttachments = color_attachments.empty() ? nullptr : color_attachments.data(),
				.pDepthAttachment = depth_att ? &*depth_att : nullptr
			};
			command.beginRendering(ri);
			pass.record_fn(ctx);
			command.endRendering();
		} else {
			pass.record_fn(ctx);
		}
	}

	const vk::ImageMemoryBarrier2 present_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
		.dstAccessMask = {},
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::ePresentSrcKHR,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = swap.images[image_index],
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	const vk::DependencyInfo end_dep{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &present_barrier
	};
	command.pipelineBarrier2(end_dep);
}
