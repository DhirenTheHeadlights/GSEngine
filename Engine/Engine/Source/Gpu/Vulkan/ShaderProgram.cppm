export module gse.gpu:vulkan_shader_program;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_descriptor_set_layout;
import :vulkan_device;
import :vulkan_pipeline_layout;
import :vulkan_shader_object;

import gse.core;

export namespace gse::gpu {
	struct dynamic_pipeline_state {
		topology topology = topology::triangle_list;
		polygon_mode polygon = polygon_mode::fill;
		cull_mode cull = cull_mode::back;
		front_face front = front_face::counter_clockwise;
		depth_state depth;
		bool depth_bias_enable = false;
		float depth_bias_constant = 0.0f;
		float depth_bias_clamp = 0.0f;
		float depth_bias_slope = 0.0f;
		bool rasterizer_discard_enable = false;
		bool primitive_restart_enable = false;
		bool depth_clamp_enable = false;
		bool alpha_to_coverage_enable = false;
		bool alpha_to_one_enable = false;
		bool logic_op_enable = false;
		sample_count samples = sample_count::e1;
		std::uint32_t sample_mask = 0xFFFFFFFFu;
		std::vector<std::uint8_t> blend_enables;
		std::vector<color_blend_equation> blend_equations;
		std::vector<color_component_flags> color_write_masks;
		std::vector<vertex_binding_desc> vertex_bindings;
		std::vector<vertex_attribute_desc> vertex_attributes;
	};
}

export namespace gse::vulkan {
	struct shader_program_create_info {
		std::span<const shader_object_create_info> stages;
		std::span<const gpu::handle<descriptor_set_layout>> set_layouts;
		std::optional<gpu::push_constant_range> push_constant_range;
		std::span<const std::uint32_t> auto_bound_sets;
		std::span<const gpu::binding_use> active_bindings;
		gpu::dynamic_pipeline_state state;
		bool is_compute = false;
		bool is_mesh = false;
	};

	class shader_program final : public non_copyable {
	public:
		shader_program() = default;

		~shader_program() override = default;

		shader_program(
			shader_program&&
		) noexcept = default;

		auto operator=(
			shader_program&&
		) noexcept -> shader_program& = default;

		[[nodiscard]]
		static auto create(
			const device& dev,
			const shader_program_create_info& info
		) -> shader_program;

		[[nodiscard]] auto layout() const -> gpu::handle<pipeline_layout>;

		[[nodiscard]] auto stages() const -> std::span<const gpu::stage_flag>;

		[[nodiscard]] auto shader_handles() const -> std::span<const gpu::handle<shader_object>>;

		[[nodiscard]] auto state() const -> const gpu::dynamic_pipeline_state&;

		[[nodiscard]] auto is_compute() const -> bool;

		[[nodiscard]] auto is_mesh() const -> bool;

		[[nodiscard]] auto bind_point() const -> gpu::bind_point;

		[[nodiscard]] auto auto_bound_sets() const -> std::span<const std::uint32_t>;

		[[nodiscard]] auto active_bindings() const -> std::span<const gpu::binding_use>;

		explicit operator bool() const;

	private:
		shader_program(
			vk::raii::PipelineLayout&& layout,
			std::vector<shader_object>&& shaders,
			std::vector<gpu::stage_flag>&& stages,
			std::vector<gpu::handle<shader_object>>&& shader_handles,
			gpu::dynamic_pipeline_state&& state,
			bool is_compute,
			bool is_mesh,
			std::vector<std::uint32_t>&& auto_bound_sets,
			std::vector<gpu::binding_use>&& active_bindings
		);

		vk::raii::PipelineLayout m_layout = nullptr;
		std::vector<shader_object> m_shaders;
		std::vector<gpu::stage_flag> m_stages;
		std::vector<gpu::handle<shader_object>> m_shader_handles;
		gpu::dynamic_pipeline_state m_state;
		bool m_is_compute = false;
		bool m_is_mesh = false;
		std::vector<std::uint32_t> m_auto_bound_sets;
		std::vector<gpu::binding_use> m_active_bindings;
	};
}

gse::vulkan::shader_program::shader_program(
	vk::raii::PipelineLayout&& layout,
	std::vector<shader_object>&& shaders,
	std::vector<gpu::stage_flag>&& stages,
	std::vector<gpu::handle<shader_object>>&& shader_handles,
	gpu::dynamic_pipeline_state&& state,
	const bool is_compute,
	const bool is_mesh,
	std::vector<std::uint32_t>&& auto_bound_sets,
	std::vector<gpu::binding_use>&& active_bindings
)
	: m_layout(std::move(layout)),
	  m_shaders(std::move(shaders)),
	  m_stages(std::move(stages)),
	  m_shader_handles(std::move(shader_handles)),
	  m_state(std::move(state)),
	  m_is_compute(is_compute),
	  m_is_mesh(is_mesh),
	  m_auto_bound_sets(std::move(auto_bound_sets)),
	  m_active_bindings(std::move(active_bindings)) {
}

auto gse::vulkan::shader_program::create(const device& dev, const shader_program_create_info& info) -> shader_program {
	std::vector<vk::DescriptorSetLayout> vk_set_layouts;
	vk_set_layouts.reserve(info.set_layouts.size());
	for (const auto h : info.set_layouts) {
		vk_set_layouts.push_back(std::bit_cast<vk::DescriptorSetLayout>(h));
	}

	std::optional<vk::PushConstantRange> vk_pc_range;
	if (info.push_constant_range.has_value()) {
		vk_pc_range = to_vk(*info.push_constant_range);
	}
	const vk::PushConstantRange* pc_ptr = vk_pc_range.has_value() ? &*vk_pc_range : nullptr;
	const std::uint32_t pc_count = vk_pc_range.has_value() ? 1u : 0u;

	vk::raii::PipelineLayout layout = dev.raii_device().createPipelineLayout({
		.setLayoutCount = static_cast<std::uint32_t>(vk_set_layouts.size()),
		.pSetLayouts = vk_set_layouts.data(),
		.pushConstantRangeCount = pc_count,
		.pPushConstantRanges = pc_ptr,
	});

	std::vector<shader_object> shaders = info.stages.size() == 1
		? [&] {
			std::vector<shader_object> single;
			single.reserve(1);
			single.emplace_back(shader_object::create(dev, info.stages[0]));
			return single;
		}()
		: shader_object::create_linked(dev, info.stages);

	std::vector<gpu::stage_flag> stages;
	stages.reserve(shaders.size());
	std::vector<gpu::handle<shader_object>> shader_handles;
	shader_handles.reserve(shaders.size());
	for (const auto& s : shaders) {
		stages.push_back(s.stage());
		shader_handles.push_back(s.handle());
	}

	return shader_program(
		std::move(layout),
		std::move(shaders),
		std::move(stages),
		std::move(shader_handles),
		gpu::dynamic_pipeline_state{ info.state },
		info.is_compute,
		info.is_mesh,
		std::vector<std::uint32_t>(info.auto_bound_sets.begin(), info.auto_bound_sets.end()),
		std::vector<gpu::binding_use>(info.active_bindings.begin(), info.active_bindings.end())
	);
}

auto gse::vulkan::shader_program::layout() const -> gpu::handle<pipeline_layout> {
	return std::bit_cast<gpu::handle<pipeline_layout>>(*m_layout);
}

auto gse::vulkan::shader_program::stages() const -> std::span<const gpu::stage_flag> {
	return m_stages;
}

auto gse::vulkan::shader_program::shader_handles() const -> std::span<const gpu::handle<shader_object>> {
	return m_shader_handles;
}

auto gse::vulkan::shader_program::state() const -> const gpu::dynamic_pipeline_state& {
	return m_state;
}

auto gse::vulkan::shader_program::is_compute() const -> bool {
	return m_is_compute;
}

auto gse::vulkan::shader_program::is_mesh() const -> bool {
	return m_is_mesh;
}

auto gse::vulkan::shader_program::bind_point() const -> gpu::bind_point {
	return m_is_compute ? gpu::bind_point::compute : gpu::bind_point::graphics;
}

auto gse::vulkan::shader_program::auto_bound_sets() const -> std::span<const std::uint32_t> {
	return m_auto_bound_sets;
}

auto gse::vulkan::shader_program::active_bindings() const -> std::span<const gpu::binding_use> {
	return m_active_bindings;
}

gse::vulkan::shader_program::operator bool() const {
	return *m_layout != nullptr;
}
