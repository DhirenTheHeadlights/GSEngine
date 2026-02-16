export module gse.graphics:geometry_renderer;

import std;

import :camera_system;
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

export namespace gse::renderer::geometry {
	struct render_data {
		std::uint32_t frame_index = 0;
		std::vector<render_queue_entry> render_queue;
		std::vector<id> render_queue_owners;
		std::vector<skinned_render_queue_entry> skinned_render_queue;
		std::vector<normal_instance_batch> normal_batches;
		std::vector<skinned_instance_batch> skinned_batches;
		std::uint32_t pending_compute_instance_count = 0;
	};

	struct state {
		context* ctx = nullptr;

		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> descriptor_sets;
		resource::handle<shader> shader_handle;

		vk::raii::Pipeline skinned_pipeline = nullptr;
		vk::raii::PipelineLayout skinned_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> skinned_descriptor_sets;
		resource::handle<shader> skinned_shader;

		std::unordered_map<std::string, per_frame_resource<vulkan::buffer_resource>> ubo_allocations;

		static constexpr std::size_t max_skin_matrices = 256 * 128;
		static constexpr std::size_t max_joints = 256;
		per_frame_resource<vulkan::buffer_resource> skin_buffer;
		per_frame_resource<std::vector<mat4>> skin_staging;

		resource::handle<shader> skin_compute;
		vk::raii::Pipeline skin_compute_pipeline = nullptr;
		vk::raii::PipelineLayout skin_compute_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> skin_compute_sets;
		vulkan::buffer_resource skeleton_buffer;
		per_frame_resource<vulkan::buffer_resource> local_pose_buffer;
		per_frame_resource<std::vector<mat4>> local_pose_staging;
		const skeleton* current_skeleton = nullptr;
		std::uint32_t current_joint_count = 0;
		bool gpu_skinning_enabled = true;

		static constexpr std::size_t max_instances = 4096;
		per_frame_resource<vulkan::buffer_resource> instance_buffer;
		per_frame_resource<std::vector<std::byte>> instance_staging;

		std::uint32_t instance_stride = 0;
		std::uint32_t batch_stride = 0;
		std::uint32_t joint_stride = 0;
		std::unordered_map<std::string, std::uint32_t> instance_offsets;
		std::unordered_map<std::string, std::uint32_t> batch_offsets;
		std::unordered_map<std::string, std::uint32_t> joint_offsets;

		static constexpr std::size_t max_batches = 256;
		per_frame_resource<vulkan::buffer_resource> normal_indirect_commands_buffer;
		per_frame_resource<vulkan::buffer_resource> skinned_indirect_commands_buffer;

		per_frame_resource<vulkan::buffer_resource> frustum_buffer;
		per_frame_resource<vulkan::buffer_resource> batch_info_buffer;
		resource::handle<shader> culling_compute;
		vk::raii::Pipeline culling_pipeline = nullptr;
		vk::raii::PipelineLayout culling_pipeline_layout = nullptr;
		per_frame_resource<vk::raii::DescriptorSet> normal_culling_descriptor_sets;
		per_frame_resource<vk::raii::DescriptorSet> skinned_culling_descriptor_sets;
		bool gpu_culling_enabled = true;

		resource::handle<texture> blank_texture;

		struct vbd_compute {
			resource::handle<shader> predict;
			resource::handle<shader> solve_color;
			resource::handle<shader> update_lambda;
			resource::handle<shader> derive_velocities;
			resource::handle<shader> sequential_passes;
			resource::handle<shader> finalize;

			vk::raii::Pipeline predict_pipeline = nullptr;
			vk::raii::Pipeline solve_color_pipeline = nullptr;
			vk::raii::Pipeline update_lambda_pipeline = nullptr;
			vk::raii::Pipeline derive_velocities_pipeline = nullptr;
			vk::raii::Pipeline sequential_passes_pipeline = nullptr;
			vk::raii::Pipeline finalize_pipeline = nullptr;

			vk::raii::PipelineLayout pipeline_layout = nullptr;
			per_frame_resource<vk::raii::DescriptorSet> descriptor_sets;
			vk::raii::QueryPool query_pool = nullptr;
			float timestamp_period = 0.f;
			float solve_ms = 0.f;
			bool descriptors_initialized = false;
		};
		mutable vbd_compute vbd;

		explicit state(context& c) : ctx(std::addressof(c)) {}
		state() = default;

		auto skin_buff() const -> const vulkan::buffer_resource& {
			return skin_buffer[ctx->config().current_frame()];
		}
	};

	auto dispatch_vbd_compute(
		const state& s,
		vk::CommandBuffer command,
		vbd::gpu_solver& solver,
		std::uint32_t frame_index
	) -> void;

	auto dispatch_skin_compute(
		const state& s,
		vk::CommandBuffer command,
		std::uint32_t instance_count,
		std::uint32_t frame_index
	) -> void;

	auto dispatch_culling_compute(
		const state& s,
		vk::CommandBuffer command,
		std::uint32_t batch_count,
		const vk::raii::DescriptorSet& culling_set,
		std::uint32_t batch_offset
	) -> void;

	auto upload_skeleton_data(
		const state& s,
		const skeleton& skel
	) -> void;

	auto filter_render_queue(
		const render_data& data,
		std::span<const id> exclude_ids
	) -> std::vector<render_queue_entry>;

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
		static auto render(render_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::geometry::filter_render_queue(const render_data& data, const std::span<const id> exclude_ids) -> std::vector<render_queue_entry> {
	std::vector<render_queue_entry> result;
	result.reserve(data.render_queue.size());

	for (std::size_t i = 0; i < data.render_queue.size(); ++i) {
		const bool excluded = std::ranges::any_of(exclude_ids, [&](const id& ex) {
			return ex == data.render_queue_owners[i];
		});

		if (!excluded) {
			result.push_back(data.render_queue[i]);
		}
	}

	return result;
}

auto gse::renderer::geometry::system::initialize(initialize_phase&, state& s) -> void {
	auto& config = s.ctx->config();

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

	s.shader_handle = s.ctx->get<shader>("Shaders/Standard3D/geometry_pass");
	s.ctx->instantly_load(s.shader_handle);
	auto descriptor_set_layouts = s.shader_handle->layouts();

	const auto camera_ubo = s.shader_handle->uniform_block("CameraUBO");

	vk::BufferCreateInfo camera_ubo_buffer_info{
		.size = camera_ubo.size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
		.sharingMode = vk::SharingMode::eExclusive
	};
	
	for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
		s.ubo_allocations["CameraUBO"][i] = config.allocator().create_buffer(camera_ubo_buffer_info);

		s.descriptor_sets[i] = s.shader_handle->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		std::unordered_map<std::string, vk::DescriptorBufferInfo> buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			}
		};

		std::unordered_map<std::string, vk::DescriptorImageInfo> image_infos = {};

		config.device_config().device.updateDescriptorSets(
			s.shader_handle->descriptor_writes(*s.descriptor_sets[i], buffer_infos, image_infos),
			nullptr
		);
	}

	const vk::PipelineLayoutCreateInfo pipeline_layout_info{
		.setLayoutCount = static_cast<std::uint32_t>(descriptor_set_layouts.size()),
		.pSetLayouts = descriptor_set_layouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	s.pipeline_layout = config.device_config().device.createPipelineLayout(pipeline_layout_info);

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

	auto shader_stages = s.shader_handle->shader_stages();

	constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = vk::False,
		.alphaToOneEnable = vk::False
	};

	auto vertex_input_info = s.shader_handle->vertex_input_state();

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
		.layout = s.pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};
	s.pipeline = config.device_config().device.createGraphicsPipeline(nullptr, pipeline_info);

	s.skinned_shader = s.ctx->get<shader>("Shaders/Standard3D/skinned_geometry_pass");
	s.ctx->instantly_load(s.skinned_shader);
	auto skinned_descriptor_set_layouts = s.skinned_shader->layouts();

	const auto instance_block = s.shader_handle->uniform_block("instanceData");
	s.instance_stride = instance_block.size;
	for (const auto& [name, member] : instance_block.members) {
		s.instance_offsets[name] = member.offset;
	}

	constexpr vk::DeviceSize skin_buffer_size = state::max_skin_matrices * sizeof(mat4);
	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		s.skin_buffer[i] = config.allocator().create_buffer(
			vk::BufferCreateInfo{
				.size = skin_buffer_size,
				.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
			}
		);
		s.skin_staging[i].reserve(state::max_skin_matrices);

		s.skinned_descriptor_sets[i] = s.skinned_shader->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		std::unordered_map<std::string, vk::DescriptorBufferInfo> skinned_buffer_infos{
			{
				"CameraUBO",
				{
					.buffer = s.ubo_allocations["CameraUBO"][i].buffer,
					.offset = 0,
					.range = camera_ubo.size
				}
			}
		};

		std::unordered_map<std::string, vk::DescriptorImageInfo> skinned_image_infos = {};

		config.device_config().device.updateDescriptorSets(
			s.skinned_shader->descriptor_writes(*s.skinned_descriptor_sets[i], skinned_buffer_infos, skinned_image_infos),
			nullptr
		);

		const vk::DeviceSize instance_buffer_size = state::max_instances * 2 * s.instance_stride;
		s.instance_buffer[i] = config.allocator().create_buffer({
			.size = instance_buffer_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		s.instance_staging[i].reserve(instance_buffer_size);

		constexpr vk::DeviceSize indirect_buffer_size = state::max_batches * sizeof(vk::DrawIndexedIndirectCommand);
		s.normal_indirect_commands_buffer[i] = config.allocator().create_buffer({
			.size = indirect_buffer_size,
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		s.skinned_indirect_commands_buffer[i] = config.allocator().create_buffer({
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

	s.skinned_pipeline_layout = config.device_config().device.createPipelineLayout(skinned_pipeline_layout_info);

	auto skinned_shader_stages = s.skinned_shader->shader_stages();
	auto skinned_vertex_input_info = s.skinned_shader->vertex_input_state();

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
		.layout = s.skinned_pipeline_layout,
		.basePipelineHandle = nullptr,
		.basePipelineIndex = 0
	};
	s.skinned_pipeline = config.device_config().device.createGraphicsPipeline(nullptr, skinned_pipeline_info);

	s.skin_compute = s.ctx->get<shader>("Shaders/Compute/skin_compute");
	s.ctx->instantly_load(s.skin_compute);

	assert(s.skin_compute->is_compute(), std::source_location::current(), "Skin compute shader is not loaded as a compute shader");

	const auto joint_block = s.skin_compute->uniform_block("skeletonData");
	s.joint_stride = joint_block.size;
	for (const auto& [name, member] : joint_block.members) {
		s.joint_offsets[name] = member.offset;
	}

	s.skeleton_buffer = config.allocator().create_buffer({
		.size = state::max_joints * s.joint_stride,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
	});

	constexpr vk::DeviceSize local_pose_size = state::max_skin_matrices * sizeof(mat4);
	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		s.local_pose_buffer[i] = config.allocator().create_buffer({
			.size = local_pose_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
		s.local_pose_staging[i].reserve(state::max_skin_matrices);

		s.skin_compute_sets[i] = s.skin_compute->descriptor_set(
			config.device_config().device,
			config.descriptor_config().pool,
			shader::set::binding_type::persistent
		);

		const vk::DescriptorBufferInfo skeleton_info{
			.buffer = s.skeleton_buffer.buffer,
			.offset = 0,
			.range = state::max_joints * s.joint_stride
		};

		const vk::DescriptorBufferInfo local_pose_info{
			.buffer = s.local_pose_buffer[i].buffer,
			.offset = 0,
			.range = local_pose_size
		};

		const vk::DescriptorBufferInfo skin_info{
			.buffer = s.skin_buffer[i].buffer,
			.offset = 0,
			.range = skin_buffer_size
		};

		std::array writes{
			vk::WriteDescriptorSet{
				.dstSet = *s.skin_compute_sets[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &skeleton_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.skin_compute_sets[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &local_pose_info
			},
			vk::WriteDescriptorSet{
				.dstSet = *s.skin_compute_sets[i],
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &skin_info
			}
		};

		config.device_config().device.updateDescriptorSets(writes, nullptr);

		auto compute_layouts = s.skin_compute->layouts();
		std::vector compute_ranges = { s.skin_compute->push_constant_range("push_constants") };

		s.skin_compute_pipeline_layout = config.device_config().device.createPipelineLayout({
			.setLayoutCount = static_cast<std::uint32_t>(compute_layouts.size()),
			.pSetLayouts = compute_layouts.data(),
			.pushConstantRangeCount = static_cast<std::uint32_t>(compute_ranges.size()),
			.pPushConstantRanges = compute_ranges.data()
		});

		s.skin_compute_pipeline = config.device_config().device.createComputePipeline(nullptr, {
			.stage = s.skin_compute->compute_stage(),
			.layout = *s.skin_compute_pipeline_layout
		});

		constexpr vk::DeviceSize frustum_size = sizeof(frustum_planes);
		s.frustum_buffer[i] = config.allocator().create_buffer({
			.size = frustum_size,
			.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
	}

	s.culling_compute = s.ctx->get<shader>("Shaders/Compute/cull_instances");
	s.ctx->instantly_load(s.culling_compute);

	const auto batch_block = s.culling_compute->uniform_block("batches");
	s.batch_stride = batch_block.size;
	for (const auto& [name, member] : batch_block.members) {
		s.batch_offsets[name] = member.offset;
	}

	for (std::size_t i = 0; i < per_frame_resource<vulkan::buffer_resource>::frames_in_flight; ++i) {
		const vk::DeviceSize batch_info_size = state::max_batches * 2 * s.batch_stride;
		s.batch_info_buffer[i] = config.allocator().create_buffer({
			.size = batch_info_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		});
	}

	if (s.culling_compute.valid() && s.culling_compute->is_compute()) {
		for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
			s.normal_culling_descriptor_sets[i] = s.culling_compute->descriptor_set(
				config.device_config().device,
				config.descriptor_config().pool,
				shader::set::binding_type::persistent
			);
			s.skinned_culling_descriptor_sets[i] = s.culling_compute->descriptor_set(
				config.device_config().device,
				config.descriptor_config().pool,
				shader::set::binding_type::persistent
			);
		}

		for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
			const vk::DescriptorBufferInfo frustum_info{
				.buffer = s.frustum_buffer[i].buffer,
				.offset = 0,
				.range = sizeof(frustum_planes)
			};

			const vk::DescriptorBufferInfo instance_info{
				.buffer = s.instance_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			const vk::DescriptorBufferInfo batch_info{
				.buffer = s.batch_info_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			const vk::DescriptorBufferInfo normal_indirect_info{
				.buffer = s.normal_indirect_commands_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			const vk::DescriptorBufferInfo skinned_indirect_info{
				.buffer = s.skinned_indirect_commands_buffer[i].buffer,
				.offset = 0,
				.range = vk::WholeSize
			};

			std::array<vk::WriteDescriptorSet, 4> normal_writes{
				vk::WriteDescriptorSet{
					.dstSet = *s.normal_culling_descriptor_sets[i],
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &frustum_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *s.normal_culling_descriptor_sets[i],
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &instance_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *s.normal_culling_descriptor_sets[i],
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &batch_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *s.normal_culling_descriptor_sets[i],
					.dstBinding = 3,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &normal_indirect_info
				}
			};

			std::array<vk::WriteDescriptorSet, 4> skinned_writes{
				vk::WriteDescriptorSet{
					.dstSet = *s.skinned_culling_descriptor_sets[i],
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &frustum_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *s.skinned_culling_descriptor_sets[i],
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &instance_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *s.skinned_culling_descriptor_sets[i],
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &batch_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *s.skinned_culling_descriptor_sets[i],
					.dstBinding = 3,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &skinned_indirect_info
				}
			};

			config.device_config().device.updateDescriptorSets(normal_writes, nullptr);
			config.device_config().device.updateDescriptorSets(skinned_writes, nullptr);
		}

		auto culling_layouts = s.culling_compute->layouts();
		std::vector culling_ranges = { s.culling_compute->push_constant_range("push_constants") };

		s.culling_pipeline_layout = config.device_config().device.createPipelineLayout({
			.setLayoutCount = static_cast<std::uint32_t>(culling_layouts.size()),
			.pSetLayouts = culling_layouts.data(),
			.pushConstantRangeCount = static_cast<std::uint32_t>(culling_ranges.size()),
			.pPushConstantRanges = culling_ranges.data()
		});

		s.culling_pipeline = config.device_config().device.createComputePipeline(nullptr, {
			.stage = s.culling_compute->compute_stage(),
			.layout = *s.culling_pipeline_layout
		});
	} else {
		s.gpu_culling_enabled = false;
	}

	s.blank_texture = s.ctx->queue<texture>("blank", unitless::vec4(1, 1, 1, 1));
	s.ctx->instantly_load(s.blank_texture);

	s.vbd.predict = s.ctx->get<shader>("Shaders/Compute/vbd_predict");
	s.vbd.solve_color = s.ctx->get<shader>("Shaders/Compute/vbd_solve_color");
	s.vbd.update_lambda = s.ctx->get<shader>("Shaders/Compute/vbd_update_lambda");
	s.vbd.derive_velocities = s.ctx->get<shader>("Shaders/Compute/vbd_derive_velocities");
	s.vbd.sequential_passes = s.ctx->get<shader>("Shaders/Compute/vbd_sequential_passes");
	s.vbd.finalize = s.ctx->get<shader>("Shaders/Compute/vbd_finalize");

	s.ctx->instantly_load(s.vbd.predict);
	s.ctx->instantly_load(s.vbd.solve_color);
	s.ctx->instantly_load(s.vbd.update_lambda);
	s.ctx->instantly_load(s.vbd.derive_velocities);
	s.ctx->instantly_load(s.vbd.sequential_passes);
	s.ctx->instantly_load(s.vbd.finalize);

	auto vbd_layouts = s.vbd.predict->layouts();
	std::vector vbd_ranges = { s.vbd.predict->push_constant_range("push_constants") };

	s.vbd.pipeline_layout = config.device_config().device.createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(vbd_layouts.size()),
		.pSetLayouts = vbd_layouts.data(),
		.pushConstantRangeCount = static_cast<std::uint32_t>(vbd_ranges.size()),
		.pPushConstantRanges = vbd_ranges.data()
	});

	auto create_vbd_pipeline = [&](const resource::handle<shader>& sh) {
		return config.device_config().device.createComputePipeline(nullptr, {
			.stage = sh->compute_stage(),
			.layout = *s.vbd.pipeline_layout
		});
	};

	s.vbd.predict_pipeline = create_vbd_pipeline(s.vbd.predict);
	s.vbd.solve_color_pipeline = create_vbd_pipeline(s.vbd.solve_color);
	s.vbd.update_lambda_pipeline = create_vbd_pipeline(s.vbd.update_lambda);
	s.vbd.derive_velocities_pipeline = create_vbd_pipeline(s.vbd.derive_velocities);
	s.vbd.sequential_passes_pipeline = create_vbd_pipeline(s.vbd.sequential_passes);
	s.vbd.finalize_pipeline = create_vbd_pipeline(s.vbd.finalize);

	s.vbd.query_pool = config.device_config().device.createQueryPool({
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = 8
	});
	static_cast<vk::Device>(*config.device_config().device).resetQueryPool(*s.vbd.query_pool, 0, 8);

	s.vbd.timestamp_period = config.device_config().physical_device.getProperties().limits.timestampPeriod;
}

auto gse::renderer::geometry::system::update(update_phase& phase, state& s) -> void {
	if (phase.registry.view<render_component>().empty()) {
		return;
	}

	const auto* cam_state = phase.try_state_of<camera::state>();
	const mat4 view_matrix = cam_state ? cam_state->view_matrix : mat4(1.0f);
	const mat4 proj_matrix = cam_state ? cam_state->projection_matrix : mat4(1.0f);

	phase.schedule([&s, &channels = phase.channels, view_matrix, proj_matrix](
		chunk<render_component> render,
		chunk<const physics::motion_component> motion,
		chunk<const physics::collision_component> collision,
		chunk<const animation_component> anim
	) {
		const auto frame_index = s.ctx->config().current_frame();

		s.shader_handle->set_uniform(
			"CameraUBO.view",
			view_matrix,
			s.ubo_allocations.at("CameraUBO")[frame_index].allocation
		);

		s.shader_handle->set_uniform(
			"CameraUBO.proj",
			proj_matrix,
			s.ubo_allocations.at("CameraUBO")[frame_index].allocation
		);

		render_data data;
		data.frame_index = frame_index;

		auto& out = data.render_queue;
		auto& owners_out = data.render_queue_owners;
		auto& skinned_out = data.skinned_render_queue;

		auto& skin_staging = s.skin_staging[frame_index];
		skin_staging.clear();

		auto& local_pose_staging = s.local_pose_staging[frame_index];
		local_pose_staging.clear();

		std::uint32_t skinned_instance_count = 0;

		for (auto& component : render) {
			if (!component.render) {
				continue;
			}

			const auto* mc = motion.find(component.owner_id());
			const auto* cc = collision.find(component.owner_id());

			for (auto& model_handle : component.model_instances) {
				if (!model_handle.handle().valid() || mc == nullptr || cc == nullptr) {
					continue;
				}

				model_handle.update(*mc, *cc);
				const auto entries = model_handle.render_queue_entries();
				out.append_range(entries);
				owners_out.resize(owners_out.size() + entries.size(), component.owner_id());
			}

			const auto* anim_comp = anim.find(component.owner_id());

			for (auto& skinned_model_handle : component.skinned_model_instances) {
				if (!skinned_model_handle.handle().valid() || mc == nullptr || cc == nullptr) {
					continue;
				}

				if (anim_comp != nullptr && !anim_comp->local_pose.empty()) {
					std::uint32_t skin_offset = 0;
					skin_offset = static_cast<std::uint32_t>(local_pose_staging.size());

					if (s.gpu_skinning_enabled && s.skin_compute.valid()) {
						local_pose_staging.insert(local_pose_staging.end(), anim_comp->local_pose.begin(), anim_comp->local_pose.end());

						if (anim_comp->skeleton && (s.current_skeleton == nullptr || s.current_skeleton->id() != anim_comp->skeleton.id())) {
							s.current_skeleton = anim_comp->skeleton.resolve();
							s.current_joint_count = static_cast<std::uint32_t>(anim_comp->skeleton->joint_count());
							upload_skeleton_data(s, *anim_comp->skeleton);
						}

						++skinned_instance_count;
					}
					else if (!anim_comp->skins.empty()) {
						skin_staging.insert(skin_staging.end(), anim_comp->skins.begin(), anim_comp->skins.end());
					}

					skinned_model_handle.update(*mc, *cc, skin_offset, s.current_joint_count);
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

	auto& instance_staging = s.instance_staging[frame_index];
	auto& normal_batches = data.normal_batches;
	auto& skinned_batches = data.skinned_batches;

	instance_staging.clear();

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
				std::byte* offset = instance_staging.data() + (global_instance_offset * s.instance_stride);
				instance_staging.resize(instance_staging.size() + s.instance_stride);
				offset = instance_staging.data() + (global_instance_offset * s.instance_stride);

				std::memcpy(offset + s.instance_offsets["model_matrix"], &inst.model_matrix, sizeof(mat4));
				std::memcpy(offset + s.instance_offsets["normal_matrix"], &inst.normal_matrix, sizeof(mat4));
				std::memcpy(offset + s.instance_offsets["skin_offset"], &inst.skin_offset, sizeof(std::uint32_t));
				std::memcpy(offset + s.instance_offsets["joint_count"], &inst.joint_count, sizeof(std::uint32_t));

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
				std::byte* offset = instance_staging.data() + (global_instance_offset * s.instance_stride);
				instance_staging.resize(instance_staging.size() + s.instance_stride);
				offset = instance_staging.data() + (global_instance_offset * s.instance_stride);

				std::memcpy(offset + s.instance_offsets["model_matrix"], &inst.model_matrix, sizeof(mat4));
				std::memcpy(offset + s.instance_offsets["normal_matrix"], &inst.normal_matrix, sizeof(mat4));
				std::memcpy(offset + s.instance_offsets["skin_offset"], &inst.skin_offset, sizeof(std::uint32_t));
				std::memcpy(offset + s.instance_offsets["joint_count"], &inst.joint_count, sizeof(std::uint32_t));

				global_instance_offset++;
			}

			skinned_batches.push_back(std::move(batch));
		}
	}

	if (!instance_staging.empty()) {
		gse::memcpy(s.instance_buffer[frame_index].allocation.mapped(), instance_staging);
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
			gse::memcpy(s.normal_indirect_commands_buffer[frame_index].allocation.mapped(), normal_indirect_commands);
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
			gse::memcpy(s.skinned_indirect_commands_buffer[frame_index].allocation.mapped(), skinned_indirect_commands);
		}
	}

	if (total_batch_count > 0 && s.gpu_culling_enabled && s.culling_compute.valid()) {
		const mat4 view_proj = proj_matrix * view_matrix;
		const frustum_planes frustum = extract_frustum_planes(view_proj);
		gse::memcpy(s.frustum_buffer[frame_index].allocation.mapped(), &frustum);

		std::byte* batch_data = s.batch_info_buffer[frame_index].allocation.mapped();

		auto write_batch_info = [&](const auto& batch, const std::size_t index) {
			std::byte* offset = batch_data + (index * s.batch_stride);

			std::memcpy(offset + s.batch_offsets["first_instance"], &batch.first_instance, sizeof(std::uint32_t));
			std::memcpy(offset + s.batch_offsets["instance_count"], &batch.instance_count, sizeof(std::uint32_t));

			const auto aabb_min = batch.world_aabb_min.template as<meters>();
			std::memcpy(offset + s.batch_offsets["aabb_min"], &aabb_min, sizeof(unitless::vec3));

			const auto aabb_max = batch.world_aabb_max.template as<meters>();
			std::memcpy(offset + s.batch_offsets["aabb_max"], &aabb_max, sizeof(unitless::vec3));
		};

		std::size_t batch_index = 0;
		for (const auto& batch : normal_batches) {
			write_batch_info(batch, batch_index++);
		}
		for (const auto& batch : skinned_batches) {
			write_batch_info(batch, batch_index++);
		}
	}

	if (s.gpu_skinning_enabled && s.skin_compute.valid() && !local_pose_staging.empty() && s.current_joint_count > 0) {
		gse::memcpy(s.local_pose_buffer[frame_index].allocation.mapped(), local_pose_staging);
		data.pending_compute_instance_count = skinned_instance_count;
	} else if (!skin_staging.empty()) {
		gse::memcpy(s.skin_buffer[frame_index].allocation.mapped(), skin_staging);
		data.pending_compute_instance_count = 0;
	} else {
		data.pending_compute_instance_count = 0;
	}

		channels.push(std::move(data));
	});
}

auto gse::renderer::geometry::system::render(render_phase& phase, const state& s) -> void {
    const auto& render_items = phase.read_channel<render_data>();
    if (render_items.empty()) {
        return;
    }

    const auto& data = render_items[0];
    const auto frame_index = data.frame_index;

    auto& config = s.ctx->config();

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

    const auto& gpu_dispatch_items = phase.read_channel<vbd::gpu_dispatch_info>();
    if (!gpu_dispatch_items.empty()) {
        auto* solver = gpu_dispatch_items[0].solver;
        if (solver) {
            if (!solver->buffers_created()) {
                solver->create_buffers(config.allocator());
            }
            solver->stage_readback(frame_index);
            if (solver->pending_dispatch()) {
                solver->commit_upload();
                dispatch_vbd_compute(s, command, *solver, frame_index);
            }
        }
    }

    if (data.pending_compute_instance_count > 0) {
        dispatch_skin_compute(s, command, data.pending_compute_instance_count, frame_index);
    }

    const auto& normal_batches = data.normal_batches;
    const auto& skinned_batches = data.skinned_batches;

    if (s.gpu_culling_enabled && s.culling_compute.valid()) {
		if (const auto& dc = s.normal_culling_descriptor_sets[frame_index]; !normal_batches.empty()) {
            dispatch_culling_compute(s, command, static_cast<std::uint32_t>(normal_batches.size()), dc.value(), 0);
        }
		if (const auto& dc = s.skinned_culling_descriptor_sets[frame_index]; !skinned_batches.empty()) {
            dispatch_culling_compute(s, command, static_cast<std::uint32_t>(skinned_batches.size()), dc.value(), static_cast<std::uint32_t>(normal_batches.size()));
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
                .buffer = s.normal_indirect_commands_buffer[frame_index].buffer,
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
                .buffer = s.skinned_indirect_commands_buffer[frame_index].buffer,
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
                    s.pipeline
                );

                const vk::DescriptorSet sets[]{ **s.descriptor_sets[frame_index] };

                command.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    s.pipeline_layout,
                    0,
                    vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
                    {}
                );

                const vk::DescriptorBufferInfo instance_buffer_info{
                    .buffer = s.instance_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = s.instance_buffer[frame_index].allocation.size()
                };

                const vk::DescriptorBufferInfo skin_buffer_info_normal{
                    .buffer = s.skin_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = s.skin_buffer[frame_index].allocation.size()
                };

                for (std::size_t i = 0; i < normal_batches.size(); ++i) {
                    const auto& batch = normal_batches[i];
                    const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

                    const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
                    const auto& tex_info = has_texture ? mesh.material()->diffuse_texture->descriptor_info() : s.blank_texture->descriptor_info();

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
                        s.pipeline_layout,
                        1,
                        descriptor_writes
                    );

                    mesh.bind(command);

                    command.drawIndexedIndirect(
                        s.normal_indirect_commands_buffer[frame_index].buffer,
                        i * sizeof(vk::DrawIndexedIndirectCommand),
                        1,
                        0
                    );
                }
            }

            if (!skinned_batches.empty()) {
                command.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    s.skinned_pipeline
                );

                const vk::DescriptorSet skinned_sets[]{ **s.skinned_descriptor_sets[frame_index] };

                command.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    s.skinned_pipeline_layout,
                    0,
                    vk::ArrayProxy<const vk::DescriptorSet>(1, skinned_sets),
                    {}
                );

                const vk::DescriptorBufferInfo skin_buffer_info{
                    .buffer = s.skin_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = s.skin_buffer[frame_index].allocation.size()
                };

                const vk::DescriptorBufferInfo skinned_instance_buffer_info{
                    .buffer = s.instance_buffer[frame_index].buffer,
                    .offset = 0,
                    .range = s.instance_buffer[frame_index].allocation.size()
                };

                for (std::size_t i = 0; i < skinned_batches.size(); ++i) {
                    const auto& batch = skinned_batches[i];
                    const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

                    const bool has_texture = mesh.material().valid() && mesh.material()->diffuse_texture.valid();
                    const auto& tex_info = has_texture ? mesh.material()->diffuse_texture->descriptor_info() : s.blank_texture->descriptor_info();

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
                        s.skinned_pipeline_layout,
                        1,
                        skinned_descriptor_writes
                    );

                    mesh.bind(command);

                    command.drawIndexedIndirect(
                        s.skinned_indirect_commands_buffer[frame_index].buffer,
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

auto gse::renderer::geometry::dispatch_skin_compute(const state& s, const vk::CommandBuffer command, const std::uint32_t instance_count, const std::uint32_t frame_index) -> void {
	if (instance_count == 0 || !s.skin_compute.valid()) {
		return;
	}

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

	command.bindPipeline(vk::PipelineBindPoint::eCompute, *s.skin_compute_pipeline);

	const vk::DescriptorSet sets[]{ *s.skin_compute_sets[frame_index] };
	command.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*s.skin_compute_pipeline_layout,
		0,
		vk::ArrayProxy<const vk::DescriptorSet>(1, sets),
		{}
	);

	s.skin_compute->push(
		command,
		*s.skin_compute_pipeline_layout,
		"push_constants",
		"joint_count", s.current_joint_count,
		"instance_count", instance_count,
		"local_pose_stride", s.current_joint_count,
		"skin_stride", s.current_joint_count
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

auto gse::renderer::geometry::dispatch_culling_compute(const state& s, const vk::CommandBuffer command, const std::uint32_t batch_count, const vk::raii::DescriptorSet& culling_set, const std::uint32_t batch_offset) -> void {
	if (batch_count == 0 || !s.gpu_culling_enabled || !s.culling_compute.valid()) {
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

	command.bindPipeline(vk::PipelineBindPoint::eCompute, *s.culling_pipeline);

	const vk::DescriptorSet culling_sets[]{ *culling_set };
	command.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*s.culling_pipeline_layout,
		0,
		vk::ArrayProxy<const vk::DescriptorSet>(1, culling_sets),
		{}
	);

	s.culling_compute->push(
		command,
		*s.culling_pipeline_layout,
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

auto gse::renderer::geometry::upload_skeleton_data(const state& s, const skeleton& skel) -> void {
	const auto joint_count = static_cast<std::size_t>(skel.joint_count());
	const auto joints = skel.joints();

	std::byte* buffer = s.skeleton_buffer.allocation.mapped();

	for (std::size_t i = 0; i < joint_count; ++i) {
		std::byte* offset = buffer + (i * s.joint_stride);

		const mat4 inverse_bind = joints[i].inverse_bind();
		const std::uint32_t parent_index = joints[i].parent_index();

		std::memcpy(offset + s.joint_offsets.at("inverse_bind"), &inverse_bind, sizeof(mat4));
		std::memcpy(offset + s.joint_offsets.at("parent_index"), &parent_index, sizeof(std::uint32_t));
	}
}

auto gse::renderer::geometry::dispatch_vbd_compute(
	const state& s,
	const vk::CommandBuffer command,
	vbd::gpu_solver& solver,
	const std::uint32_t frame_index
) -> void {
	auto& vbd = s.vbd;
	auto& config = s.ctx->config();

	if (!vbd.descriptors_initialized) {
		for (std::size_t i = 0; i < per_frame_resource<vk::raii::DescriptorSet>::frames_in_flight; ++i) {
			vbd.descriptor_sets.resources()[i] = vbd.predict->descriptor_set(
				config.device_config().device,
				config.descriptor_config().pool,
				shader::set::binding_type::persistent
			);

			const vk::DescriptorBufferInfo body_info{
				.buffer = solver.body_buffer().buffer,
				.offset = 0,
				.range = solver.body_buffer().allocation.size()
			};
			const vk::DescriptorBufferInfo contact_info{
				.buffer = solver.contact_buffer().buffer,
				.offset = 0,
				.range = solver.contact_buffer().allocation.size()
			};
			const vk::DescriptorBufferInfo motor_info{
				.buffer = solver.motor_buffer().buffer,
				.offset = 0,
				.range = solver.motor_buffer().allocation.size()
			};
			const vk::DescriptorBufferInfo color_info{
				.buffer = solver.color_buffer().buffer,
				.offset = 0,
				.range = solver.color_buffer().allocation.size()
			};
			const vk::DescriptorBufferInfo map_info{
				.buffer = solver.body_contact_map_buffer().buffer,
				.offset = 0,
				.range = solver.body_contact_map_buffer().allocation.size()
			};
			const vk::DescriptorBufferInfo solve_info{
				.buffer = solver.solve_state_buffer().buffer,
				.offset = 0,
				.range = solver.solve_state_buffer().allocation.size()
			};

			auto& ds = vbd.descriptor_sets.resources()[i];
			std::array writes{
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &body_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &contact_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &motor_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 3,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &color_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 4,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &map_info
				},
				vk::WriteDescriptorSet{
					.dstSet = *ds,
					.dstBinding = 5,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eStorageBuffer,
					.pBufferInfo = &solve_info
				}
			};

			config.device_config().device.updateDescriptorSets(writes, nullptr);
		}

		vbd.descriptors_initialized = true;
	}

	const auto body_count = solver.body_count();
	const auto contact_count = solver.contact_count();
	const auto motor_count = solver.motor_count();

	if (body_count == 0) return;

	if (solver.frame_count() >= 2) {
		const std::uint32_t prev_query_base = (frame_index % 2) * 4;
		std::array<std::uint64_t, 2> timestamps{};
		const vk::Device device= *config.device_config().device;
		auto result = device.getQueryPoolResults(
			*vbd.query_pool, prev_query_base, 2,
			sizeof(timestamps), timestamps.data(), sizeof(std::uint64_t),
			vk::QueryResultFlagBits::e64
		);
		if (result == vk::Result::eSuccess) {
			vbd.solve_ms = static_cast<float>(timestamps[1] - timestamps[0]) * vbd.timestamp_period * 1e-6f;
		}
	}

	const std::uint32_t query_base = (frame_index % 2) * 4;
	command.resetQueryPool(*vbd.query_pool, query_base, 4);

	constexpr vk::MemoryBarrier2 host_to_compute{
		.srcStageMask = vk::PipelineStageFlagBits2::eHost,
		.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
	};
	constexpr vk::MemoryBarrier2 prev_transfer_to_transfer{
		.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite
	};
	constexpr vk::MemoryBarrier2 init_barriers[] = { host_to_compute, prev_transfer_to_transfer };
	command.pipelineBarrier2({ .memoryBarrierCount = 2, .pMemoryBarriers = init_barriers });

	const vk::DescriptorSet sets[] = { *vbd.descriptor_sets[frame_index % 2] };

	constexpr vk::MemoryBarrier2 compute_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
	};
	const vk::DependencyInfo compute_dep{ .memoryBarrierCount = 1, .pMemoryBarriers = &compute_barrier };

	command.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, *vbd.query_pool, query_base);

	const auto& cfg = solver.solver_cfg();
	const float sub_dt = solver.dt() / static_cast<float>(cfg.substeps);
	const float h_squared = sub_dt * sub_dt;

	auto ceil_div = [](const std::uint32_t a, const std::uint32_t b) { return (a + b - 1) / b; };

	auto bind_and_push = [&](const resource::handle<shader>& sh, const vk::raii::Pipeline& pipeline,
		std::uint32_t color_offset, std::uint32_t color_count,
		std::uint32_t substep, std::uint32_t iteration) {
		command.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
		command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *vbd.pipeline_layout, 0, { 1, sets }, {});
		sh->push(command, *vbd.pipeline_layout, "push_constants",
			"body_count", body_count,
			"contact_count", contact_count,
			"motor_count", motor_count,
			"color_offset", color_offset,
			"color_count", color_count,
			"h_squared", h_squared,
			"dt", sub_dt,
			"rho", cfg.rho,
			"linear_damping", cfg.linear_damping,
			"velocity_sleep_threshold", cfg.velocity_sleep_threshold,
			"substep", substep,
			"iteration", iteration
		);
	};

	for (std::uint32_t substep = 0; substep < cfg.substeps; ++substep) {
		bind_and_push(vbd.predict, vbd.predict_pipeline, 0u, 0u, substep, 0u);
		command.dispatch(ceil_div(body_count, vbd::workgroup_size), 1, 1);
		command.pipelineBarrier2(compute_dep);

		for (std::uint32_t iter = 0; iter < cfg.iterations; ++iter) {
			for (const auto& [offset, count] : solver.color_ranges()) {
				bind_and_push(vbd.solve_color, vbd.solve_color_pipeline, offset, count, substep, iter);
				command.dispatch(ceil_div(count, vbd::workgroup_size), 1, 1);
				command.pipelineBarrier2(compute_dep);
			}

			if (solver.motor_only_count() > 0) {
				bind_and_push(vbd.solve_color, vbd.solve_color_pipeline,
					solver.motor_only_offset(), solver.motor_only_count(), substep, iter);
				command.dispatch(ceil_div(solver.motor_only_count(), vbd::workgroup_size), 1, 1);
				command.pipelineBarrier2(compute_dep);
			}
		}

		if (contact_count > 0) {
			bind_and_push(vbd.update_lambda, vbd.update_lambda_pipeline, 0u, 0u, substep, 0u);
			command.dispatch(ceil_div(contact_count, vbd::workgroup_size), 1, 1);
			command.pipelineBarrier2(compute_dep);
		}

		bind_and_push(vbd.derive_velocities, vbd.derive_velocities_pipeline, 0u, 0u, substep, 0u);
		command.dispatch(ceil_div(body_count, vbd::workgroup_size), 1, 1);
		command.pipelineBarrier2(compute_dep);

		bind_and_push(vbd.sequential_passes, vbd.sequential_passes_pipeline, 0u, 0u, substep, 0u);
		command.dispatch(1, 1, 1);
		command.pipelineBarrier2(compute_dep);

		bind_and_push(vbd.finalize, vbd.finalize_pipeline, 0u, 0u, substep, 0u);
		command.dispatch(ceil_div(body_count, vbd::workgroup_size), 1, 1);
		command.pipelineBarrier2(compute_dep);
	}

	command.writeTimestamp2(vk::PipelineStageFlagBits2::eComputeShader, *vbd.query_pool, query_base + 1);

	constexpr vk::MemoryBarrier2 compute_to_transfer{
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead
	};
	command.pipelineBarrier2({ .memoryBarrierCount = 1, .pMemoryBarriers = &compute_to_transfer });

	const vk::DeviceSize body_copy_size = body_count * sizeof(vbd::gpu_body);
	command.copyBuffer(solver.body_buffer().buffer, solver.readback_buffers()[frame_index % 2].buffer,
		vk::BufferCopy{ .srcOffset = 0, .dstOffset = 0, .size = body_copy_size });

	if (contact_count > 0) {
		constexpr vk::DeviceSize lambda_src_offset = sizeof(std::uint32_t) * 2 + sizeof(float);
		const vk::DeviceSize lambda_dst_base = vbd::max_bodies * sizeof(vbd::gpu_body);
		for (std::uint32_t ci = 0; ci < contact_count; ++ci) {
			command.copyBuffer(solver.contact_buffer().buffer, solver.readback_buffers()[frame_index % 2].buffer,
				vk::BufferCopy{
					.srcOffset = ci * sizeof(vbd::gpu_contact) + lambda_src_offset,
					.dstOffset = lambda_dst_base + ci * sizeof(float),
					.size = sizeof(float)
				});
		}
	}

	constexpr vk::MemoryBarrier2 compute_to_host{
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eHost,
		.dstAccessMask = vk::AccessFlagBits2::eHostRead
	};
	command.pipelineBarrier2({ .memoryBarrierCount = 1, .pMemoryBarriers = &compute_to_host });

	solver.mark_dispatched(frame_index);
}
