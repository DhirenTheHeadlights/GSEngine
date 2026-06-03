export module gse.vulkan:shader_program;

import std;
import vulkan;

import :handles;
import :types;
import :device;
import :pipeline_layout;
import :shader_object;

import gse.core;

export namespace gse::vulkan {
	struct shader_program_create_info {
		std::span<const shader_object_create_info> stages;
		std::optional<gpu::push_constant_range> push_constant_range;
		gpu::dynamic_pipeline_state state;
		bool is_compute = false;
		bool is_mesh = false;
	};

	class shader_program final : public non_copyable {
	public:
		shader_program() {}
		~shader_program() = default;

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

		[[nodiscard]] auto layout() const -> gpu::pipeline_layout_handle;

		[[nodiscard]] auto stages() const -> std::span<const gpu::stage_flag>;

		[[nodiscard]] auto shader_handles() const -> std::span<const gpu::shader_object_handle>;

		[[nodiscard]] auto state() const -> const gpu::dynamic_pipeline_state&;

		[[nodiscard]] auto is_compute() const -> bool;

		[[nodiscard]] auto is_mesh() const -> bool;

		[[nodiscard]] auto bind_point() const -> gpu::bind_point;

		[[nodiscard]] auto valid() const -> bool;

	private:
		shader_program(
			vk::raii::PipelineLayout&& layout,
			std::vector<shader_object>&& shaders,
			std::vector<gpu::stage_flag>&& stages,
			std::vector<gpu::shader_object_handle>&& shader_handles,
			gpu::dynamic_pipeline_state&& state,
			bool is_compute,
			bool is_mesh
		);

		vk::raii::PipelineLayout m_layout = nullptr;
		std::vector<shader_object> m_shaders;
		std::vector<gpu::stage_flag> m_stages;
		std::vector<gpu::shader_object_handle> m_shader_handles;
		gpu::dynamic_pipeline_state m_state;
		bool m_is_compute = false;
		bool m_is_mesh = false;
	};
}

gse::vulkan::shader_program::shader_program(vk::raii::PipelineLayout&& layout, std::vector<shader_object>&& shaders, std::vector<gpu::stage_flag>&& stages, std::vector<gpu::shader_object_handle>&& shader_handles, gpu::dynamic_pipeline_state&& state, const bool is_compute, const bool is_mesh)
	: m_layout(std::move(layout)), m_shaders(std::move(shaders)), m_stages(std::move(stages)), m_shader_handles(std::move(shader_handles)), m_state(std::move(state)), m_is_compute(is_compute), m_is_mesh(is_mesh) {
}

auto gse::vulkan::shader_program::create(const device& dev, const shader_program_create_info& info) -> shader_program {
	std::optional<vk::PushConstantRange> vk_pc_range;
	if (info.push_constant_range.has_value()) {
		vk_pc_range = to_vk(*info.push_constant_range);
	}
	const vk::PushConstantRange* pc_ptr = vk_pc_range.has_value() ? &*vk_pc_range : nullptr;
	const std::uint32_t pc_count = vk_pc_range.has_value() ? 1u : 0u;

	vk::raii::PipelineLayout layout = dev.raii_device().createPipelineLayout({
		.pushConstantRangeCount = pc_count,
		.pPushConstantRanges = pc_ptr,
	});

	std::vector<shader_object> shaders = info.stages.size() == 1 ? [&] {
		std::vector<shader_object> single;
		single.reserve(1);
		single.emplace_back(shader_object::create(dev, info.stages[0]));
		return single;
	}()
																 : shader_object::create_linked(dev, info.stages);

	std::vector<gpu::stage_flag> stages;
	stages.reserve(shaders.size());
	std::vector<gpu::shader_object_handle> shader_handles;
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
		info.is_mesh
	);
}

auto gse::vulkan::shader_program::layout() const -> gpu::pipeline_layout_handle {
	return std::bit_cast<gpu::pipeline_layout_handle>(*m_layout);
}

auto gse::vulkan::shader_program::stages() const -> std::span<const gpu::stage_flag> {
	return m_stages;
}

auto gse::vulkan::shader_program::shader_handles() const -> std::span<const gpu::shader_object_handle> {
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

auto gse::vulkan::shader_program::valid() const -> bool {
	return *m_layout != nullptr;
}
