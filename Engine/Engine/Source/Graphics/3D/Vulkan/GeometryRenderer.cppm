export module gse.graphics:geometry_renderer;

import std;

import :camera;
import :mesh;
import :skinned_mesh;
import :model;
import :skinned_model;
import :render_component;
import :animation_component;
import :material;
import :shader;
import :skeleton;
import :texture;

import gse.physics.math;
import gse.utility;
import gse.platform;
import gse.physics;

namespace gse::renderer {
	using frustum_planes = std::array<unitless::vec4, 6>;

	auto transform_aabb(const vec3<length>& local_min, const vec3<length>& local_max, const mat4& model_matrix) -> std::pair<vec3<length>, vec3<length>> {
		const std::array corners = {
			unitless::vec4(local_min.as<meters>(), 1.0f),
			unitless::vec4(local_max.x().as<meters>(), local_min.y().as<meters>(), local_min.z().as<meters>(), 1.0f),
			unitless::vec4(local_min.x().as<meters>(), local_max.y().as<meters>(), local_min.z().as<meters>(), 1.0f),
			unitless::vec4(local_max.x().as<meters>(), local_max.y().as<meters>(), local_min.z().as<meters>(), 1.0f),
			unitless::vec4(local_min.x().as<meters>(), local_min.y().as<meters>(), local_max.z().as<meters>(), 1.0f),
			unitless::vec4(local_max.x().as<meters>(), local_min.y().as<meters>(), local_max.z().as<meters>(), 1.0f),
			unitless::vec4(local_min.x().as<meters>(), local_max.y().as<meters>(), local_max.z().as<meters>(), 1.0f),
			unitless::vec4(local_max.as<meters>(), 1.0f)
		};

		vec3<length> world_min(meters(std::numeric_limits<float>::max()));
		vec3<length> world_max(meters(std::numeric_limits<float>::lowest()));

		for (const auto& corner : corners) {
			const unitless::vec4 world_corner = model_matrix * corner;
			const vec3<length> world_pos(
				meters(world_corner.x()),
				meters(world_corner.y()),
				meters(world_corner.z())
			);

			world_min = vec3<length>(
				std::min(world_min.x(), world_pos.x()),
				std::min(world_min.y(), world_pos.y()),
				std::min(world_min.z(), world_pos.z())
			);

			world_max = vec3<length>(
				std::max(world_max.x(), world_pos.x()),
				std::max(world_max.y(), world_pos.y()),
				std::max(world_max.z(), world_pos.z())
			);
		}

		return { world_min, world_max };
	}

	auto extract_frustum_planes(const mat4& view_proj) -> frustum_planes {
		frustum_planes planes;

		planes[0] = unitless::vec4(
			view_proj[0][3] + view_proj[0][0],
			view_proj[1][3] + view_proj[1][0],
			view_proj[2][3] + view_proj[2][0],
			view_proj[3][3] + view_proj[3][0]
		);

		planes[1] = unitless::vec4(
			view_proj[0][3] - view_proj[0][0],
			view_proj[1][3] - view_proj[1][0],
			view_proj[2][3] - view_proj[2][0],
			view_proj[3][3] - view_proj[3][0]
		);

		planes[2] = unitless::vec4(
			view_proj[0][3] + view_proj[0][1],
			view_proj[1][3] + view_proj[1][1],
			view_proj[2][3] + view_proj[2][1],
			view_proj[3][3] + view_proj[3][1]
		);

		planes[3] = unitless::vec4(
			view_proj[0][3] - view_proj[0][1],
			view_proj[1][3] - view_proj[1][1],
			view_proj[2][3] - view_proj[2][1],
			view_proj[3][3] - view_proj[3][1]
		);

		planes[4] = unitless::vec4(
			view_proj[0][3] + view_proj[0][2],
			view_proj[1][3] + view_proj[1][2],
			view_proj[2][3] + view_proj[2][2],
			view_proj[3][3] + view_proj[3][2]
		);

		planes[5] = unitless::vec4(
			view_proj[0][3] - view_proj[0][2],
			view_proj[1][3] - view_proj[1][2],
			view_proj[2][3] - view_proj[2][2],
			view_proj[3][3] - view_proj[3][2]
		);

		for (auto& plane : planes) {
			if (const float length = magnitude(plane); length > 0.0f) {
				plane = plane / length;
			}
		}

		return planes;
	}

	struct normal_batch_key {
		const model* model_ptr;
		std::size_t mesh_index;

		auto operator==(const normal_batch_key& other) const -> bool {
			return model_ptr == other.model_ptr && mesh_index == other.mesh_index;
		}
	};

	struct normal_batch_key_hash {
		auto operator()(const normal_batch_key& key) const -> std::size_t {
			return std::hash<const void*>{}(key.model_ptr) ^ std::hash<std::size_t>{}(key.mesh_index);
		}
	};


	struct normal_instance_batch {
		normal_batch_key key;
		std::uint32_t first_instance;
		std::uint32_t instance_count;
		vec3<length> world_aabb_min;
		vec3<length> world_aabb_max;
	};

	struct skinned_batch_key {
		const skinned_model* model_ptr;
		std::size_t mesh_index;

		auto operator==(const skinned_batch_key& other) const -> bool {
			return model_ptr == other.model_ptr && mesh_index == other.mesh_index;
		}
	};

	struct skinned_batch_key_hash {
		auto operator()(const skinned_batch_key& key) const -> std::size_t {
			return std::hash<const void*>{}(key.model_ptr) ^ std::hash<std::size_t>{}(key.mesh_index);
		}
	};


	struct skinned_instance_batch {
		skinned_batch_key key;
		std::uint32_t first_instance;
		std::uint32_t instance_count;
		vec3<length> world_aabb_min;
		vec3<length> world_aabb_max;
	};
}

export namespace gse::renderer {
	class geometry final : public gse::system {
	public:
		explicit geometry(
			context& context
		);

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		auto render(
		) -> void override;

		auto render_queue(
		) const -> std::span<const render_queue_entry>;

		auto render_queue_excluding(
			std::span<const id> exclude_ids
		) const -> std::vector<render_queue_entry>;

		auto skinned_render_queue(
		) const -> std::span<const skinned_render_queue_entry>;

		auto skin_buffer(
		) const -> const vulkan::buffer_resource&;
	private:
		context& m_context;

		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> m_descriptor_sets;
		resource::handle<shader> m_shader;

		vk::raii::Pipeline m_skinned_pipeline = nullptr;
		vk::raii::PipelineLayout m_skinned_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> m_skinned_descriptor_sets;
		resource::handle<shader> m_skinned_shader;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> m_ubo_allocations;

		static constexpr std::size_t max_skin_matrices = 256 * 128;
		static constexpr std::size_t max_joints = 256;
		per_frame_resource<vulkan::buffer_resource> m_skin_buffer;
		per_frame_resource<std::vector<mat4>> m_skin_staging;

		resource::handle<shader> m_skin_compute;
		vk::raii::Pipeline m_skin_compute_pipeline = nullptr;
		vk::raii::PipelineLayout m_skin_compute_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> m_skin_compute_sets;
		vulkan::buffer_resource m_skeleton_buffer;
		per_frame_resource<vulkan::buffer_resource> m_local_pose_buffer;
		per_frame_resource<std::vector<mat4>> m_local_pose_staging;
		const skeleton* m_current_skeleton = nullptr;
		std::uint32_t m_current_joint_count = 0;
		bool m_gpu_skinning_enabled = true;
		per_frame_resource<std::uint32_t> m_pending_compute_instance_count;

		static constexpr std::size_t max_instances = 4096;
		per_frame_resource<vulkan::buffer_resource> m_instance_buffer;
		per_frame_resource<std::vector<std::byte>> m_instance_staging;
		per_frame_resource<std::vector<normal_instance_batch>> m_normal_instance_batches;
		per_frame_resource<std::vector<skinned_instance_batch>> m_skinned_instance_batches;

		std::uint32_t m_instance_stride = 0;
		std::uint32_t m_batch_stride = 0;
		std::uint32_t m_joint_stride = 0;
		std::unordered_map<std::string, std::uint32_t> m_instance_offsets;
		std::unordered_map<std::string, std::uint32_t> m_batch_offsets;
		std::unordered_map<std::string, std::uint32_t> m_joint_offsets;

		static constexpr std::size_t max_batches = 256;
		per_frame_resource<vulkan::buffer_resource> m_normal_indirect_commands_buffer;
		per_frame_resource<vulkan::buffer_resource> m_skinned_indirect_commands_buffer;

		per_frame_resource<vulkan::buffer_resource> m_frustum_buffer;
		per_frame_resource<vulkan::buffer_resource> m_batch_info_buffer;
		resource::handle<shader> m_culling_compute;
		vk::raii::Pipeline m_culling_pipeline = nullptr;
		vk::raii::PipelineLayout m_culling_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> m_normal_culling_descriptor_sets;
		per_frame_resource<vk::raii::DescriptorSet> m_skinned_culling_descriptor_sets;
		bool m_gpu_culling_enabled = true;

		double_buffer<std::vector<render_queue_entry>> m_render_queue;
		double_buffer<std::vector<id>> m_render_queue_owners;
		double_buffer<std::vector<skinned_render_queue_entry>> m_skinned_render_queue;

		resource::handle<texture> m_blank_texture;

		auto dispatch_skin_compute(
			vk::CommandBuffer command,
			std::uint32_t instance_count
		) -> void;

		auto dispatch_culling_compute(
			vk::CommandBuffer command,
			std::uint32_t batch_count,
			const vk::raii::DescriptorSet& culling_set,
			std::uint32_t batch_offset
		) const -> void;

		auto upload_skeleton_data(
			const skeleton& skel
		) const -> void;
	};
}

gse::renderer::geometry::geometry(context& context) : m_context(context) {}

auto gse::renderer::geometry::initialize() -> void {
	auto& config = m_context.config();

	auto transition_gbuffer_images = [](vulkan::config& cfg) {
		cfg.add_transient_work([&cfg](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
			auto transition_to_general = [&cmd](
				vulkan::image_resource& img,
				const vk::ImageAspectFlags aspect,
				const vk::PipelineStageFlags2 dst_stage,
				const vk::AccessFlags2 dst_access
			) {
				const vk::ImageMemoryBarrier2 barrier{
					.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					.srcAccessMask = {},
					.dstStageMask = dst_stage,
					.dstAccessMask = dst_access,
					.oldLayout = vk::ImageLayout::eUndefined,
					.newLayout = vk::ImageLayout::eGeneral,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.image = img.image,
					.subresourceRange = {
						.aspectMask = aspect,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};

				const vk::DependencyInfo dep{
					.imageMemoryBarrierCount = 1,
					.pImageMemoryBarriers = &barrier
				};

				cmd.pipelineBarrier2(dep);
				img.current_layout = vk::ImageLayout::eGeneral;
			};

			transition_to_general(
				cfg.swap_chain_config().albedo_image,
				vk::ImageAspectFlagBits::eColor,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
			);

			transition_to_general(
				cfg.swap_chain_config().normal_image,
				vk::ImageAspectFlagBits::eColor,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
			);

			transition_to_general(
				cfg.swap_chain_config().depth_image,
				vk::ImageAspectFlagBits::eDepth,
				vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead
			);

			return {};
		});
	};

	transition_gbuffer_images(config);
	config.on_swap_chain_recreate(transition_gbuffer_images);

	m_shader = m_context.get<shader>("geometry_pass");
	m_context.instantly_load(m_shader);
	auto descriptor_set_layouts = m_shader->layouts();

	const auto camera_ubo = m_shader->uniform_block("CameraUBO");

	vk::BufferCreateInfo camera_ubo_buffer_info{
		.size = camera_ubo.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};
	
	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		m_ubo_allocations["CameraUBO"][i] = config.allocator().create_buffer(camera_ubo_buffer_info);

		m_descriptor_sets[i] = m_shader->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = m_ubo_allocations["CameraUBO"][i].buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			}
		};

		std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

		config.device_config().device.updateDescriptorSets(
			m_shader->descriptor_writes(*m_descriptor_sets[i], buffer_infos, image_infos),
			nullptr
		);
	}

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
		.pSetLayouts = descriptor_set_layouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	m_pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

	constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.depthBiasConstantFactor = 2.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 2.0f,
		.lineWidth = 1.0f
	};

	constexpr vk::PipelineDepthStencilStateCreateInfo depth_stencil{
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	const std::array g_buffer_color_formats = {
		config.swap_chain_config().albedo_image.format,
		config.swap_chain_config().normal_image.format
	};

	std::array<vk::PipelineColorBlendAttachmentState, g_buffer_color_formats.size()> color_blend_attachments;
	color_blend_attachments.fill({
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	});

	const vk::PipelineColorBlendStateCreateInfo color_blending{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = static_cast<std::uint32_t>(color_blend_attachments.size()),
		.pAttachments = color_blend_attachments.data()
	};

	auto shader_stages = m_shader->shader_stages();

	constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	auto vertex_input_info = m_shader->vertex_input_state();

	const vk::PipelineRenderingCreateInfoKHR geometry_rendering_info = {
		.colorAttachmentCount = static_cast<uint32_t>(g_buffer_color_formats.size()),
		.pColorAttachmentFormats = g_buffer_color_formats.data(),
		.depthAttachmentFormat = vk::Format::eD32Sfloat
	};

	constexpr std::array dynamic_states = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	const vk::PipelineDynamicStateCreateInfo dynamic_state{
		.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
	};

	constexpr vk::PipelineViewportStateCreateInfo viewport_state{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
	};

	const vk::GraphicsPipelineCreateInfo pipeline_info{
		.pNext = &geometry_rendering_info,
		.stageCount = static_cast<std::uint32_t>(shader_stages.size()),
		.pStages = shader_stages.data(),
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state,
		.layout = m_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};
	m_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

	m_skinned_shader = m_context.get<shader>("skinned_geometry_pass");
	m_context.instantly_load(m_skinned_shader);
	auto skinned_descriptor_set_layouts = m_skinned_shader->layouts();

	const auto instance_block = m_shader->uniform_block("instanceData");
	m_instance_stride = instance_block.size;
	for (const auto& [name, member] : instance_block.members) {
		m_instance_offsets[name] = member.offset;
	}

	constexpr vk::DeviceSize skin_buffer_size = max_skin_matrices * sizeof(mat4);
	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		m_skin_buffer[i] = config.allocator().create_buffer(
			vk::BufferCreateInfo{
				.size = skin_buffer_size,
				.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
			}
		);
		m_skin_staging[i].reserve(max_skin_matrices);

		m_skinned_descriptor_sets[i] = m_skinned_shader->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		std::unordered_map<std::string, vk::DescriptorBufferInfo> skinned_buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = m_ubo_allocations["CameraUBO"][i].buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			}
		};

		std::unordered_map<std::string, vk::DescriptorImageInfo> skinned_image_infos = {};

		config.device_config().device.updateDescriptorSets(
			m_skinned_shader->descriptor_writes(*m_skinned_descriptor_sets[i], skinned_buffer_infos, skinned_image_infos),
			nullptr
		);

		const vk::DeviceSize instance_buffer_size = max_instances * 2 * m_instance_stride;
		m_instance_buffer[i] = config.allocator().create_buffer({
			.size = instance_buffer_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		m_instance_staging[i].reserve(instance_buffer_size);

		constexpr vk::DeviceSize indirect_buffer_size = max_batches * sizeof(vk::DrawIndexedIndirectCommand);
		m_normal_indirect_commands_buffer[i] = config.allocator().create_buffer({
			.size = indirect_buffer_size,
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		m_skinned_indirect_commands_buffer[i] = config.allocator().create_buffer({
			.size = indirect_buffer_size,
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
	}

	const vk::PipelineLayoutCreateInfo skinned_pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(skinned_descriptor_set_layouts.size()),
		.pSetLayouts = skinned_descriptor_set_layouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	m_skinned_pipeline_layout = config.device_config().device.createPipelineLayout(skinned_pipeline_layout_info);

	auto skinned_shader_stages = m_skinned_shader->shader_stages();
	auto skinned_vertex_input_info = m_skinned_shader->vertex_input_state();

	const vk::GraphicsPipelineCreateInfo skinned_pipeline_info{
		.pNext = &geometry_rendering_info,
		.stageCount = static_cast<std::uint32_t>(skinned_shader_stages.size()),
		.pStages = skinned_shader_stages.data(),
		.pVertexInputState = &skinned_vertex_input_info,
		.pInputAssemblyState = &input_assembly,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state,
		.layout = m_skinned_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};
	m_skinned_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, skinned_pipeline_info);

	m_skin_compute = m_context.get<shader>("skin_compute");
	m_context.instantly_load(m_skin_compute);

	assert(m_skin_compute->is_compute(), std::source_location::current(), "Skin compute shader is not loaded as a compute shader");

	const auto joint_block = m_skin_compute->uniform_block("skeletonData");
	m_joint_stride = joint_block.size;
	for (const auto& [name, member] : joint_block.members) {
		m_joint_offsets[name] = member.offset;
	}

	m_skeleton_buffer = config.allocator().create_buffer({
		.size = max_joints * m_joint_stride,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
	});

	constexpr vk::DeviceSize local_pose_size = max_skin_matrices * sizeof(mat4);
	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		m_local_pose_buffer[i] = config.allocator().create_buffer({
			.size = local_pose_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		m_local_pose_staging[i].reserve(max_skin_matrices);

		m_skin_compute_sets[i] = m_skin_compute->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		const vk::DescriptorBufferInfo skeleton_info{
			.buffer = m_skeleton_buffer.buffer,
			.offset = 0,
			.range = max_joints * m_joint_stride
		};

		const vk::DescriptorBufferInfo local_pose_info{
			.buffer = m_local_pose_buffer[i].buffer,
			.offset = 0,
			.range = local_pose_size
		};

		const vk::DescriptorBufferInfo skin_info{
			.buffer = m_skin_buffer[i].buffer,
			.offset = 0,
			.range = skin_buffer_size
		};

		std::array writes{
			vk::WriteDescriptorSet{
				.dstSet = *m_skin_compute_sets[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &skeleton_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *m_skin_compute_sets[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &local_pose_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *m_skin_compute_sets[i],
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &skin_info
			}
		};

		config.device_config().device.updateDescriptorSets(writes, nullptr);

		auto compute_layouts = m_skin_compute->layouts();
		std::vector compute_ranges = { m_skin_compute->push_constant_range("push_constants") };

		m_skin_compute_pipeline_layout = config.device_config().device.createPipelineLayout({
			.setLayoutCount = static_cast<std::uint32_t>(compute_layouts.size()),
			.pSetLayouts = compute_layouts.data(),
			.pushConstantRangeCount = static_cast<std::uint32_t>(compute_ranges.size()),
			.pPushConstantRanges = compute_ranges.data()
		});

		m_skin_compute_pipeline = config.device_config().device.createComputePipeline(nullptr, {
			.stage = m_skin_compute->compute_stage(),
			.layout = *m_skin_compute_pipeline_layout
		});

		constexpr vk::DeviceSize frustum_size = sizeof(frustum_planes);
		m_frustum_buffer[i] = config.allocator().create_buffer({
			.size = frustum_size,
			.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
	}

	m_culling_compute = m_context.get<shader>("cull_instances");
	m_context.instantly_load(m_culling_compute);

	const auto batch_block = m_culling_compute->uniform_block("batches");
	m_batch_stride = batch_block.size;
	for (const auto& [name, member] : batch_block.members) {
		m_batch_offsets[name] = member.offset;
	}

	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		const vk::DeviceSize batch_info_size = max_batches * 2 * m_batch_stride;
		m_batch_info_buffer[i] = config.allocator().create_buffer({
			.size = batch_info_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
	}

	if (m_culling_compute.valid() && m_culling_compute->is_compute()) {
		for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
			m_normal_culling_descriptor_sets[i] = m_culling_compute->descriptor_set(
				config.device_config().device,
				config.descriptor_config().pool,
				shader::set::binding_type::persistent
			);
			m_skinned_culling_descriptor_sets[i] = m_culling_compute->descriptor_set(
				config.device_config().device,
				config.descriptor_config().pool,
				shader::set::binding_type::persistent
			);
		}

		for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
			const vk::DescriptorBufferInfo frustum_info{
				.buffer = m_frustum_buffer[i].buffer,
				.offset = 0,
				.range = sizeof(frustum_planes)
			};

			const vk::DescriptorBufferInfo instance_info{
				.buffer = m_instance_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			const vk::DescriptorBufferInfo batch_info{
				.buffer = m_batch_info_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			const vk::DescriptorBufferInfo normal_indirect_info{
				.buffer = m_normal_indirect_commands_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			const vk::DescriptorBufferInfo skinned_indirect_info{
				.buffer = m_skinned_indirect_commands_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			std::array<vk::WriteDescriptorSet, 4> normal_writes{
				vk::WriteDescriptorSet{
					.dstSet = *m_normal_culling_descriptor_sets[i],
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &frustum_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *m_normal_culling_descriptor_sets[i],
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &instance_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *m_normal_culling_descriptor_sets[i],
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &batch_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *m_normal_culling_descriptor_sets[i],
					.dstBinding = 3,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &normal_indirect_info
				}
			};

			std::array<vk::WriteDescriptorSet, 4> skinned_writes{
				vk::WriteDescriptorSet{
					.dstSet = *m_skinned_culling_descriptor_sets[i],
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &frustum_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *m_skinned_culling_descriptor_sets[i],
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &instance_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *m_skinned_culling_descriptor_sets[i],
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &batch_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *m_skinned_culling_descriptor_sets[i],
					.dstBinding = 3,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &skinned_indirect_info
				}
			};

			config.device_config().device.updateDescriptorSets(normal_writes, nullptr);
			config.device_config().device.updateDescriptorSets(skinned_writes, nullptr);
		}

		auto culling_layouts = m_culling_compute->layouts();
		std::vector culling_ranges = { m_culling_compute->push_constant_range("push_constants") };

		m_culling_pipeline_layout = config.device_config().device.createPipelineLayout({
			.setLayoutCount = static_cast<std::uint32_t>(culling_layouts.size()),
			.pSetLayouts = culling_layouts.data(),
			.pushConstantRangeCount = static_cast<std::uint32_t>(culling_ranges.size()),
			.pPushConstantRanges = culling_ranges.data()
		});

		m_culling_pipeline = config.device_config().device.createComputePipeline(nullptr, {
			.stage = m_culling_compute->compute_stage(),
			.layout = *m_culling_pipeline_layout
		});
	} else {
		m_gpu_culling_enabled = false;
	}

	m_blank_texture = m_context.queue<texture>("blank", unitless::vec4(1, 1, 1, 1));
	m_context.instantly_load(m_blank_texture);

	frame_sync::on_end([this] {
		m_render_queue.flip();
		m_render_queue_owners.flip();
		m_skinned_render_queue.flip();
	});
}

auto gse::renderer::geometry::update() -> void {
	this->write([this](
		const component_chunk<render_component>& render_chunk,
		const component_chunk<animation_component>& anim_chunk
	) {
		if (render_chunk.empty()) {
			return;
		}

		const auto frame_index = m_context.config().current_frame();

		m_shader->set_uniform(
			"CameraUBO.view",
			m_context.camera().view(),
			m_ubo_allocations.at("CameraUBO")[frame_index].allocation
		);

		m_shader->set_uniform(
			"CameraUBO.proj",
			m_context.camera().projection(m_context.window().viewport()),
			m_ubo_allocations.at("CameraUBO")[frame_index].allocation
		);

		auto& out = m_render_queue.write();
		out.clear();

		auto& owners_out = m_render_queue_owners.write();
		owners_out.clear();

		auto& skinned_out = m_skinned_render_queue.write();
		skinned_out.clear();

		auto& skin_staging = m_skin_staging[frame_index];
		skin_staging.clear();

		auto& local_pose_staging = m_local_pose_staging[frame_index];
		local_pose_staging.clear();

		std::uint32_t skinned_instance_count = 0;

		for (auto& component : render_chunk) {
			if (!component.render) {
				continue;
			}

			const auto* mc = render_chunk.read_from<physics::motion_component>(component);
			const auto* cc = render_chunk.read_from<physics::collision_component>(component);

			for (auto& model_handle : component.model_instances) {
				if (!model_handle.handle().valid() || mc == nullptr || cc == nullptr) {
					continue;
				}

				model_handle.update(*mc, *cc);
				const auto entries = model_handle.render_queue_entries();
				out.append_range(entries);
				owners_out.resize(owners_out.size() + entries.size(), component.owner_id());
			}

			const auto* anim = anim_chunk.read(component.owner_id());

			for (auto& skinned_model_handle : component.skinned_model_instances) {
				if (!skinned_model_handle.handle().valid() || mc == nullptr || cc == nullptr) {
					continue;
				}

				if (anim != nullptr && !anim->local_pose.empty()) {
					std::uint32_t skin_offset = 0;
					skin_offset = static_cast<std::uint32_t>(local_pose_staging.size());

					if (m_gpu_skinning_enabled && m_skin_compute.valid()) {
						local_pose_staging.insert(local_pose_staging.end(), anim->local_pose.begin(), anim->local_pose.end());

						if (anim->skeleton && (m_current_skeleton == nullptr || m_current_skeleton->id() != anim->skeleton.id())) {
							m_current_skeleton = anim->skeleton.resolve();
							m_current_joint_count = static_cast<std::uint32_t>(anim->skeleton->joint_count());
							upload_skeleton_data(*anim->skeleton);
						}

						++skinned_instance_count;
					}
					else if (!anim->skins.empty()) {
						skin_staging.insert(skin_staging.end(), anim->skins.begin(), anim->skins.end());
					}

					skinned_model_handle.update(*mc, *cc, skin_offset, m_current_joint_count);
					const auto entries = skinned_model_handle.render_queue_entries();
					skinned_out.append_range(entries);
				}
			}
		}

		std::ranges::sort(
			out,
			[](const render_queue_entry& a, const render_queue_entry& b) {
				const auto* ma = a.model.resolve();
				const auto* mb = b.model.resolve();

				if (ma != mb) {
					return ma < mb;
				}

				return a.index < b.index;
			}
		);

		std::ranges::sort(
			skinned_out,
			[](const skinned_render_queue_entry& a, const skinned_render_queue_entry& b) {
				const auto* ma = a.model.resolve();
				const auto* mb = b.model.resolve();

				if (ma != mb) {
					return ma < mb;
				}

				return a.index < b.index;
			}
		);

		struct instance_data {
			mat4 model_matrix;
			mat4 normal_matrix;
			std::uint32_t skin_offset;
			std::uint32_t joint_count;
		};

		auto& instance_staging = m_instance_staging[frame_index];
		auto& normal_batches = m_normal_instance_batches[frame_index];
		auto& skinned_batches = m_skinned_instance_batches[frame_index];

		instance_staging.clear();
		normal_batches.clear();
		skinned_batches.clear();

		std::uint32_t global_instance_offset = 0;

		if (!out.empty()) {
			std::unordered_map<normal_batch_key, std::vector<instance_data>, normal_batch_key_hash> normal_batch_map;

			for (const auto& entry : out) {
				const normal_batch_key key{
					.model_ptr = entry.model.resolve(),
					.mesh_index = entry.index
				};

				instance_data inst{
					.model_matrix = entry.model_matrix,
					.normal_matrix = entry.normal_matrix,
					.skin_offset = 0,
					.joint_count = 0
				};

				normal_batch_map[key].push_back(inst);
			}

			for (auto& [key, instances] : normal_batch_map) {
				const auto& mesh = key.model_ptr->meshes()[key.mesh_index];
				const auto [local_aabb_min, local_aabb_max] = mesh.aabb();
				const std::uint32_t instance_count = static_cast<std::uint32_t>(instances.size());

				vec3<length> world_aabb_min(meters(std::numeric_limits<float>::max()));
				vec3<length> world_aabb_max(meters(std::numeric_limits<float>::lowest()));

				for (const auto& inst : instances) {
					const auto [inst_min, inst_max] = transform_aabb(local_aabb_min, local_aabb_max, inst.model_matrix);

					world_aabb_min = vec3<length>(
						std::min(world_aabb_min.x(), inst_min.x()),
						std::min(world_aabb_min.y(), inst_min.y()),
						std::min(world_aabb_min.z(), inst_min.z())
					);

					world_aabb_max = vec3<length>(
						std::max(world_aabb_max.x(), inst_max.x()),
						std::max(world_aabb_max.y(), inst_max.y()),
						std::max(world_aabb_max.z(), inst_max.z())
					);
				}

				normal_instance_batch batch{
					.key = key,
					.first_instance = global_instance_offset,
					.instance_count = instance_count,
					.world_aabb_min = world_aabb_min,
					.world_aabb_max = world_aabb_max
				};

				for (const auto& inst : instances) {
					std::byte* offset = instance_staging.data() + (global_instance_offset * m_instance_stride);
					instance_staging.resize(instance_staging.size() + m_instance_stride);
					offset = instance_staging.data() + (global_instance_offset * m_instance_stride);

					std::memcpy(offset + m_instance_offsets["model_matrix"], &inst.model_matrix, sizeof(mat4));
					std::memcpy(offset + m_instance_offsets["normal_matrix"], &inst.normal_matrix, sizeof(mat4));
					std::memcpy(offset + m_instance_offsets["skin_offset"], &inst.skin_offset, sizeof(std::uint32_t));
					std::memcpy(offset + m_instance_offsets["joint_count"], &inst.joint_count, sizeof(std::uint32_t));

					global_instance_offset++;
				}
				normal_batches.push_back(std::move(batch));
			}
		}

			if (!skinned_out.empty()) {
				std::unordered_map<skinned_batch_key, std::vector<instance_data>, skinned_batch_key_hash> skinned_batch_map;

				for (const auto& entry : skinned_out) {
					const skinned_batch_key key{
						.model_ptr = entry.model.resolve(),
						.mesh_index = entry.index
					};

					instance_data inst{
						.model_matrix = entry.model_matrix,
						.normal_matrix = entry.normal_matrix,
						.skin_offset = entry.skin_offset,
						.joint_count = entry.joint_count
					};

					skinned_batch_map[key].push_back(inst);
				}

				for (auto& [key, instances] : skinned_batch_map) {
					const auto& mesh = key.model_ptr->meshes()[key.mesh_index];
					const auto [local_aabb_min, local_aabb_max] = mesh.aabb();
					const std::uint32_t instance_count = static_cast<std::uint32_t>(instances.size());

					vec3<length> world_aabb_min(meters(std::numeric_limits<float>::max()));
					vec3<length> world_aabb_max(meters(std::numeric_limits<float>::lowest()));

					for (const auto& inst : instances) {
						const auto [inst_min, inst_max] = transform_aabb(local_aabb_min, local_aabb_max, inst.model_matrix);

						world_aabb_min = vec3<length>(
							std::min(world_aabb_min.x(), inst_min.x()),
							std::min(world_aabb_min.y(), inst_min.y()),
							std::min(world_aabb_min.z(), inst_min.z())
						);

						world_aabb_max = vec3<length>(
							std::max(world_aabb_max.x(), inst_max.x()),
							std::max(world_aabb_max.y(), inst_max.y()),
							std::max(world_aabb_max.z(), inst_max.z())
						);
					}

					skinned_instance_batch batch{
						.key = key,
						.first_instance = global_instance_offset,
						.instance_count = instance_count,
						.world_aabb_min = world_aabb_min,
						.world_aabb_max = world_aabb_max
					};

					for (const auto& inst : instances) {
						std::byte* offset = instance_staging.data() + (global_instance_offset * m_instance_stride);
						instance_staging.resize(instance_staging.size() + m_instance_stride);
						offset = instance_staging.data() + (global_instance_offset * m_instance_stride);

						std::memcpy(offset + m_instance_offsets["model_matrix"], &inst.model_matrix, sizeof(mat4));
						std::memcpy(offset + m_instance_offsets["normal_matrix"], &inst.normal_matrix, sizeof(mat4));
						std::memcpy(offset + m_instance_offsets["skin_offset"], &inst.skin_offset, sizeof(std::uint32_t));
						std::memcpy(offset + m_instance_offsets["joint_count"], &inst.joint_count, sizeof(std::uint32_t));

						global_instance_offset++;
					}

					skinned_batches.push_back(std::move(batch));
				}
			}

			if (!instance_staging.empty()) {
				gse::memcpy(m_instance_buffer[frame_index].allocation.mapped(), instance_staging);
			}

			const std::size_t total_batch_count = normal_batches.size() + skinned_batches.size();

			if (!normal_batches.empty()) {
				std::vector<vk::DrawIndexedIndirectCommand> normal_indirect_commands;
				normal_indirect_commands.reserve(normal_batches.size());

				for (const auto& batch : normal_batches) {
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					vk::DrawIndexedIndirectCommand cmd{
						.indexCount = static_cast<std::uint32_t>(mesh.indices().size()),
						.instanceCount = batch.instance_count,
						.firstIndex = 0,
						.vertexOffset = 0,
						.firstInstance = batch.first_instance
					};

					normal_indirect_commands.push_back(cmd);
				}

				if (!normal_indirect_commands.empty()) {
					gse::memcpy(m_normal_indirect_commands_buffer[frame_index].allocation.mapped(), normal_indirect_commands);
				}
			}

			if (!skinned_batches.empty()) {
				std::vector<vk::DrawIndexedIndirectCommand> skinned_indirect_commands;
				skinned_indirect_commands.reserve(skinned_batches.size());

				for (const auto& batch : skinned_batches) {
					const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

					vk::DrawIndexedIndirectCommand cmd{
						.indexCount = static_cast<std::uint32_t>(mesh.indices().size()),
						.instanceCount = batch.instance_count,
						.firstIndex = 0,
						.vertexOffset = 0,
						.firstInstance = batch.first_instance
					};

					skinned_indirect_commands.push_back(cmd);
				}

				if (!skinned_indirect_commands.empty()) {
					gse::memcpy(m_skinned_indirect_commands_buffer[frame_index].allocation.mapped(), skinned_indirect_commands);
				}
			}

			if (total_batch_count > 0 && m_gpu_culling_enabled && m_culling_compute.valid()) {
				const mat4 view_proj = m_context.camera().projection(m_context.window().viewport()) * m_context.camera().view();
				const frustum_planes frustum = extract_frustum_planes(view_proj);
				gse::memcpy(m_frustum_buffer[frame_index].allocation.mapped(), &frustum);

				std::byte* batch_data = m_batch_info_buffer[frame_index].allocation.mapped();

				auto write_batch_info = [&](const auto& batch, const std::size_t index) {
					std::byte* offset = batch_data + (index * m_batch_stride);

					std::memcpy(offset + m_batch_offsets["first_instance"], &batch.first_instance, sizeof(std::uint32_t));
					std::memcpy(offset + m_batch_offsets["instance_count"], &batch.instance_count, sizeof(std::uint32_t));

					const auto aabb_min = batch.world_aabb_min.template as<meters>();
					std::memcpy(offset + m_batch_offsets["aabb_min"], &aabb_min, sizeof(unitless::vec3));

					const auto aabb_max = batch.world_aabb_max.template as<meters>();
					std::memcpy(offset + m_batch_offsets["aabb_max"], &aabb_max, sizeof(unitless::vec3));
				};

				std::size_t batch_index = 0;
				for (const auto& batch : normal_batches) {
					write_batch_info(batch, batch_index++);
				}
				for (const auto& batch : skinned_batches) {
					write_batch_info(batch, batch_index++);
				}
			}

		if (m_gpu_skinning_enabled && m_skin_compute.valid() && !local_pose_staging.empty() && m_current_joint_count > 0) {
			gse::memcpy(m_local_pose_buffer[frame_index].allocation.mapped(), local_pose_staging);
			m_pending_compute_instance_count[frame_index] = skinned_instance_count;
		} else if (!skin_staging.empty()) {
			gse::memcpy(m_skin_buffer[frame_index].allocation.mapped(), skin_staging);
			m_pending_compute_instance_count[frame_index] = 0;
		} else {
			m_pending_compute_instance_count[frame_index] = 0;
		}
	});
}

auto gse::renderer::geometry::render() -> void {
    auto& config = m_context.config();

    if (!config.frame_in_progress()) {
        return;
    }

    const auto command = config.frame_context().command_buffer;

    constexpr vk::MemoryBarrier2 pre_render_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
    };

    const vk::DependencyInfo pre_dep{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &pre_render_barrier
    };

    command.pipelineBarrier2(pre_dep);

    std::array color_attachments{
        vk::RenderingAttachmentInfo{
            .imageView = config.swap_chain_config().albedo_image.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                .color = vk::ClearColorValue{
                    .float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f }
                }
            }
        },
        vk::RenderingAttachmentInfo{
            .imageView = config.swap_chain_config().normal_image.view,
            .imageLayout = vk::ImageLayout::eGeneral,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                .color = vk::ClearColorValue{
                    .float32 = std::array{ 0.0f, 0.0f, 0.0f, 1.0f }
                }
            }
        }
    };

    vk::RenderingAttachmentInfo depth_attachment{
        .imageView = config.swap_chain_config().depth_image.view,
        .imageLayout = vk::ImageLayout::eGeneral,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearValue{
            .depthStencil = vk::ClearDepthStencilValue{
                .depth = 1.0f
            }
        }
    };

    const vk::RenderingInfo rendering_info{
        .renderArea = { { 0, 0 }, config.swap_chain_config().extent },
        .layerCount = 1,
        .colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size()),
        .pColorAttachments = color_attachments.data(),
        .pDepthAttachment = &depth_attachment
    };

    const auto frame_index = config.current_frame();

    if (m_pending_compute_instance_count[frame_index] > 0) {
        dispatch_skin_compute(command, m_pending_compute_instance_count[frame_index]);
    }

    const auto& normal_batches = m_normal_instance_batches[frame_index];
    const auto& skinned_batches = m_skinned_instance_batches[frame_index];

    if (m_gpu_culling_enabled && m_culling_compute.valid()) {
		if (const auto& dc = m_normal_culling_descriptor_sets[frame_index]; !normal_batches.empty()) {
            dispatch_culling_compute(command, static_cast<std::uint32_t>(normal_batches.size()), dc.value(), 0);
        }
		if (const auto& dc = m_skinned_culling_descriptor_sets[frame_index]; !skinned_batches.empty()) {
            dispatch_culling_compute(command, static_cast<std::uint32_t>(skinned_batches.size()), dc.value(), static_cast<std::uint32_t>(normal_batches.size()));
        }
    } else {
        std::vector<vk::BufferMemoryBarrier2> indirect_barriers;

        if (!normal_batches.empty()) {
            indirect_barriers.push_back(vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eHost,
                .srcAccessMask = vk::AccessFlagBits2::eHostWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
                .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .buffer = m_normal_indirect_commands_buffer[frame_index].buffer,
                .offset = 0,
                .size = vk::WholeSize
            });
        }

        if (!skinned_batches.empty()) {
            indirect_barriers.push_back(vk::BufferMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eHost,
                .srcAccessMask = vk::AccessFlagBits2::eHostWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
                .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .buffer = m_skinned_indirect_commands_buffer[frame_index].buffer,
                .offset = 0,
                .size = vk::WholeSize
            });
        }

        if (!indirect_barriers.empty()) {
            const vk::DependencyInfo indirect_dep{
                .bufferMemoryBarrierCount = static_cast<std::uint32_t>(indirect_barriers.size()),
                .pBufferMemoryBarriers = indirect_barriers.data()
            };
            command.pipelineBarrier2(indirect_dep);
        }
    }

    vulkan::render(
        config,
        rendering_info,
        [&] {
            const vk::Viewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(config.swap_chain_config().extent.width),
                .height = static_cast<float>(config.swap_chain_config().extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };
            command.setViewport(0, viewport);

            const vk::Rect2D scissor{
                .offset = { 0, 0 },
                .extent = config.swap_chain_config().extent
            };
            command.setScissor(0, scissor);

            if (!normal_batches.empty()) {
                command.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    m_pipeline
                );

                const vk::DescriptorSet sets[]{ **m_descriptor_sets[frame_index] };

                command.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    m_pipeline_layout,
                    0,
                    vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
                    {}
                );

                const vk::DescriptorBufferInfo instance_buffer_info{
                    .buffer = m_instance_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = m_instance_buffer[frame_index].allocation.size()
                };

                const vk::DescriptorBufferInfo skin_buffer_info_normal{
                    .buffer = m_skin_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = m_skin_buffer[frame_index].allocation.size()
                };

                for (std::size_t i = 0; i < normal_batches.size(); ++i) {
                    const auto& batch = normal_batches[i];
                    const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

                    const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
                    const auto& tex_info = has_texture ? mesh.material()->diffuse_texture->descriptor_info() : m_blank_texture->descriptor_info();

                    const std::array<vk::WriteDescriptorSet, 3> descriptor_writes{
                        vk::WriteDescriptorSet{
                            .dstBinding = 2,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .pImageInfo = &tex_info
                        },
                        vk::WriteDescriptorSet{
                            .dstBinding = 3,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &skin_buffer_info_normal
                        },
                        vk::WriteDescriptorSet{
                            .dstBinding = 4,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &instance_buffer_info
                        }
                    };

                    command.pushDescriptorSetKHR(
                        vk::PipelineBindPoint::eGraphics,
                        m_pipeline_layout,
                        1,
                        descriptor_writes
                    );

                    mesh.bind(command);

                    command.drawIndexedIndirect(
                        m_normal_indirect_commands_buffer[frame_index].buffer,
                        i * sizeof(vk::DrawIndexedIndirectCommand),
                        1,
                        0
                    );
                }
            }

            if (!skinned_batches.empty()) {
                command.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    m_skinned_pipeline
                );

                const vk::DescriptorSet skinned_sets[]{ **m_skinned_descriptor_sets[frame_index] };

                command.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    m_skinned_pipeline_layout,
                    0,
                    vk::ArrayProxy<const vk::DescriptorSet>(1, skinned_sets),
                    {}
                );

                const vk::DescriptorBufferInfo skin_buffer_info{
                    .buffer = m_skin_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = m_skin_buffer[frame_index].allocation.size()
                };

                const vk::DescriptorBufferInfo skinned_instance_buffer_info{
                    .buffer = m_instance_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = m_instance_buffer[frame_index].allocation.size()
                };

                for (std::size_t i = 0; i < skinned_batches.size(); ++i) {
                    const auto& batch = skinned_batches[i];
                    const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

                    const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
                    const auto& tex_info = has_texture ? mesh.material()->diffuse_texture->descriptor_info() : m_blank_texture->descriptor_info();

                    const std::array<vk::WriteDescriptorSet, 3> skinned_descriptor_writes{
                        vk::WriteDescriptorSet{
                            .dstBinding = 2,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                            .pImageInfo = &tex_info
                        },
                        vk::WriteDescriptorSet{
                            .dstBinding = 3,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &skin_buffer_info
                        },
                        vk::WriteDescriptorSet{
                            .dstBinding = 4,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &skinned_instance_buffer_info
                        }
                    };

                    command.pushDescriptorSetKHR(
                        vk::PipelineBindPoint::eGraphics,
                        m_skinned_pipeline_layout,
                        1,
                        skinned_descriptor_writes
                    );

                    mesh.bind(command);

                    command.drawIndexedIndirect(
                        m_skinned_indirect_commands_buffer[frame_index].buffer,
                        i * sizeof(vk::DrawIndexedIndirectCommand),
                        1,
                        0
                    );
                }
            }
        }
    );

    constexpr vk::MemoryBarrier2 post_render_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eLateFragmentTests,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead
    };

    const vk::DependencyInfo post_dep{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &post_render_barrier
    };

    command.pipelineBarrier2(post_dep);
}

auto gse::renderer::geometry::render_queue() const -> std::span<const render_queue_entry> {
	return m_render_queue.read();
}

auto gse::renderer::geometry::render_queue_excluding(const std::span<const id> exclude_ids) const -> std::vector<render_queue_entry> {
	const auto& entries = m_render_queue.read();
	const auto& owners = m_render_queue_owners.read();

	std::vector<render_queue_entry> result;
	result.reserve(entries.size());

	for (std::size_t i = 0; i < entries.size(); ++i) {
		bool excluded = std::ranges::any_of(exclude_ids, [&](const id& ex) { return ex == owners[i]; });
		if (!excluded) {
			result.push_back(entries[i]);
		}
	}

	return result;
}

auto gse::renderer::geometry::skinned_render_queue() const -> std::span<const skinned_render_queue_entry> {
	return m_skinned_render_queue.read();
}

auto gse::renderer::geometry::skin_buffer() const -> const vulkan::buffer_resource& {
	return m_skin_buffer[m_context.config().current_frame()];
}

auto gse::renderer::geometry::dispatch_skin_compute(const vk::CommandBuffer command, const std::uint32_t instance_count) -> void {
	if (instance_count == 0 || !m_skin_compute.valid()) {
		return;
	}

	const auto frame_index = m_context.config().current_frame();

	constexpr vk::MemoryBarrier2 host_to_device_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eHost,
		.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead
	};

	const vk::DependencyInfo host_dep{
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &host_to_device_barrier
	};

	command.pipelineBarrier2(host_dep);

	command.bindPipeline(vk::PipelineBindPoint::eCompute, *m_skin_compute_pipeline);

	const vk::DescriptorSet sets[]{ *m_skin_compute_sets[frame_index] };
	command.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*m_skin_compute_pipeline_layout,
		0,
		vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
		{}
	);

	m_skin_compute->push(
		command,
		*m_skin_compute_pipeline_layout,
		"push_constants",
		"joint_count", m_current_joint_count,
		"instance_count", instance_count,
		"local_pose_stride", m_current_joint_count,
		"skin_stride", m_current_joint_count
	);

	command.dispatch(instance_count, 1, 1);

	constexpr vk::MemoryBarrier2 compute_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eVertexShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead
	};

	const vk::DependencyInfo compute_dep{
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &compute_barrier
	};

	command.pipelineBarrier2(compute_dep);
}

auto gse::renderer::geometry::dispatch_culling_compute(const vk::CommandBuffer command, const std::uint32_t batch_count, const vk::raii::DescriptorSet& culling_set, const std::uint32_t batch_offset) const -> void {
	if (batch_count == 0 || !m_gpu_culling_enabled || !m_culling_compute.valid()) {
		return;
	}

	constexpr vk::MemoryBarrier2 host_to_compute_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eHost,
		.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eUniformRead
	};

	const vk::DependencyInfo host_dep{
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &host_to_compute_barrier
	};

	command.pipelineBarrier2(host_dep);

	command.bindPipeline(vk::PipelineBindPoint::eCompute, *m_culling_pipeline);

	const vk::DescriptorSet culling_sets[]{ *culling_set };
	command.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*m_culling_pipeline_layout,
		0,
		vk::ArrayProxy<const vk::DescriptorSet>(1, culling_sets),
		{}
	);

	m_culling_compute->push(
		command,
		*m_culling_pipeline_layout,
		"push_constants",
		"batch_offset", batch_offset
	);

	command.dispatch(batch_count, 1, 1);

	constexpr vk::MemoryBarrier2 compute_to_indirect_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
		.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead
	};

	const vk::DependencyInfo compute_dep{
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &compute_to_indirect_barrier
	};

	command.pipelineBarrier2(compute_dep);
}

auto gse::renderer::geometry::upload_skeleton_data(const skeleton& skel) const -> void {
	const auto joint_count = static_cast<std::size_t>(skel.joint_count());
	const auto joints = skel.joints();

	std::byte* buffer = m_skeleton_buffer.allocation.mapped();

	for (std::size_t i = 0; i < joint_count; ++i) {
		std::byte* offset = buffer + (i * m_joint_stride);

		const mat4 inverse_bind = joints[i].inverse_bind();
		const std::uint32_t parent_index = joints[i].parent_index();

		std::memcpy(offset + m_joint_offsets.at("inverse_bind"), &inverse_bind, sizeof(mat4));
		std::memcpy(offset + m_joint_offsets.at("parent_index"), &parent_index, sizeof(std::uint32_t));
	}
}
