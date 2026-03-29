export module gse.platform:render_graph;

import std;

import gse.assert;
import gse.platform.vulkan;
import gse.utility;

export namespace gse::vulkan {
	class render_graph;

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
			const vk::Viewport& viewport
		) -> void;

		auto set_scissor(
			const vk::Rect2D& scissor
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

	private:
		friend class render_graph;
		vk::CommandBuffer m_cmd;
		explicit recording_context(vk::CommandBuffer cmd);
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

	auto attachment(
		const image_resource& img,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto swapchain_write(
	) -> resource_usage;

	struct upload_ref {
		const void* ptr = nullptr;
		vk::PipelineStageFlags2 dst_stage = vk::PipelineStageFlagBits2::eComputeShader;
	};

	auto upload(
		const buffer_resource& buf
	) -> upload_ref;

	struct graphics_pass_info {
		std::vector<vk::RenderingAttachmentInfo> color_attachments;
		std::optional<vk::RenderingAttachmentInfo> depth_attachment;
		vk::Rect2D render_area;
		std::uint32_t layer_count = 1;
	};

	struct render_pass_data {
		std::type_index pass_type = typeid(void);
		std::vector<resource_usage> reads;
		std::vector<resource_usage> writes;
		std::vector<upload_ref> uploads;
		std::vector<std::type_index> after_passes;
		bool enabled = true;
		std::optional<graphics_pass_info> graphics_info;
		std::move_only_function<void(recording_context&)> record_fn;
	};

	class pass_builder {
	public:
		pass_builder(pass_builder&&) = default;
		auto operator=(pass_builder&&) -> pass_builder& = default;
		~pass_builder();

		template <typename... Args>
		auto reads(
			Args&&... args
		) -> pass_builder&&;

		template <typename... Args>
		auto writes(
			Args&&... args
		) -> pass_builder&&;

		template <typename... Args>
		auto uploads(
			Args&&... args
		) -> pass_builder&&;

		template <typename T>
		auto after(
		) -> pass_builder&&;

		auto when(
			bool condition
		) -> pass_builder&&;

		auto record(
			std::move_only_function<void(recording_context&)> fn
		) -> void;

		auto record_graphics(
			vk::RenderingInfo info,
			std::move_only_function<void(recording_context&)> fn
		) -> void;

	private:
		friend class render_graph;
		explicit pass_builder(
			render_graph& graph,
			std::type_index pass_type
		);

		auto submit(
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

auto gse::vulkan::recording_context::set_viewport(const vk::Viewport& viewport) -> void {
	m_cmd.setViewport(0, viewport);
}

auto gse::vulkan::recording_context::set_scissor(const vk::Rect2D& scissor) -> void {
	m_cmd.setScissor(0, scissor);
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

auto gse::vulkan::sampled(const image_resource& img, vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(img), .type = resource_type::image }, stage, vk::AccessFlagBits2::eShaderSampledRead };
}

auto gse::vulkan::storage(const buffer_resource& buf, vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite };
}

auto gse::vulkan::storage_read(const buffer_resource& buf, vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eShaderStorageRead };
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

auto gse::vulkan::swapchain_write() -> resource_usage {
	return {
		{ .ptr = &swapchain_sentinel, .type = resource_type::image },
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
	};
}

auto gse::vulkan::upload(const buffer_resource& buf) -> upload_ref {
	return { std::addressof(buf) };
}

gse::vulkan::pass_builder::pass_builder(render_graph& graph, std::type_index pass_type)
	: m_graph(std::addressof(graph)), m_pass{ .pass_type = pass_type } {}

gse::vulkan::pass_builder::~pass_builder() {
	assert(m_submitted, std::source_location::current(), "pass_builder destroyed without calling record() or record_graphics()");
}

template <typename... Args>
auto gse::vulkan::pass_builder::reads(Args&&... args) -> pass_builder&& {
	(m_pass.reads.push_back(std::forward<Args>(args)), ...);
	return std::move(*this);
}

template <typename... Args>
auto gse::vulkan::pass_builder::writes(Args&&... args) -> pass_builder&& {
	(m_pass.writes.push_back(std::forward<Args>(args)), ...);
	return std::move(*this);
}

template <typename... Args>
auto gse::vulkan::pass_builder::uploads(Args&&... args) -> pass_builder&& {
	(m_pass.uploads.push_back(std::forward<Args>(args)), ...);
	return std::move(*this);
}

template <typename T>
auto gse::vulkan::pass_builder::after() -> pass_builder&& {
	m_pass.after_passes.push_back(std::type_index(typeid(T)));
	return std::move(*this);
}

auto gse::vulkan::pass_builder::when(const bool condition) -> pass_builder&& {
	m_pass.enabled = condition;
	return std::move(*this);
}

auto gse::vulkan::pass_builder::record(std::move_only_function<void(recording_context&)> fn) -> void {
	m_pass.record_fn = std::move(fn);
	m_submitted = true;
	submit();
}

auto gse::vulkan::pass_builder::record_graphics(vk::RenderingInfo info, std::move_only_function<void(recording_context&)> fn) -> void {
	graphics_pass_info gpi{
		.render_area = info.renderArea,
		.layer_count = info.layerCount
	};

	if (info.pColorAttachments && info.colorAttachmentCount > 0) {
		gpi.color_attachments.assign(info.pColorAttachments, info.pColorAttachments + info.colorAttachmentCount);
	}

	if (info.pDepthAttachment) {
		gpi.depth_attachment = *info.pDepthAttachment;
	}

	m_pass.graphics_info = std::move(gpi);
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

	for (std::size_t i = 0; i < m_passes.size(); ++i) {
		for (const auto& dep : m_passes[i].after_passes) {
			if (auto it = type_to_index.find(dep); it != type_to_index.end()) {
				adj[it->second].push_back(i);
				++in_degree[i];
			}
		}
	}

	for (std::size_t i = 0; i < m_passes.size(); ++i) {
		for (std::size_t j = i + 1; j < m_passes.size(); ++j) {
			bool dependency = false;
			for (const auto& w : m_passes[i].writes) {
				for (const auto& r : m_passes[j].reads) {
					if (w.resource.ptr == r.resource.ptr) dependency = true;
				}
				for (const auto& r2 : m_passes[j].writes) {
					if (w.resource.ptr == r2.resource.ptr) dependency = true;
				}
			}
			if (dependency) {
				bool already = false;
				for (const auto n : adj[i]) {
					if (n == j) { already = true; break; }
				}
				if (!already) {
					adj[i].push_back(j);
					++in_degree[j];
				}
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

		if (!pass.uploads.empty()) {
			auto dst_stage = vk::PipelineStageFlags2{};
			for (const auto& u : pass.uploads) {
				dst_stage |= u.dst_stage;
			}

			barriers.push_back({
				.srcStageMask = vk::PipelineStageFlagBits2::eHost,
				.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
				.dstStageMask = dst_stage,
				.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eUniformRead
			});
		}

		for (std::size_t pi = 0; pi < si; ++pi) {
			const auto& prev = m_passes[sorted[pi]];
			for (const auto& [prev_resource, prev_stage, prev_access] : prev.writes) {
				for (const auto& [read_resource, read_stage, read_access] : pass.reads) {
					if (prev_resource.ptr == read_resource.ptr) {
						barriers.push_back({
							.srcStageMask = prev_stage,
							.srcAccessMask = prev_access,
							.dstStageMask = read_stage,
							.dstAccessMask = read_access
						});
					}
				}
				for (const auto& [cur_resource, cur_stage, cur_access] : pass.writes) {
					if (prev_resource.ptr == cur_resource.ptr) {
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

		if (pass.graphics_info) {
			auto& gpi = *pass.graphics_info;
			const vk::RenderingInfo ri{
				.renderArea = gpi.render_area,
				.layerCount = gpi.layer_count,
				.colorAttachmentCount = static_cast<std::uint32_t>(gpi.color_attachments.size()),
				.pColorAttachments = gpi.color_attachments.empty() ? nullptr : gpi.color_attachments.data(),
				.pDepthAttachment = gpi.depth_attachment ? &*gpi.depth_attachment : nullptr
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
