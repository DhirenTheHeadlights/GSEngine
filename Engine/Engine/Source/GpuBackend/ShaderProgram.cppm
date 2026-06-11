export module gse.gpu_backend:shader_program;

import std;

import :core;
import :enums;
import :pipeline;

import gse.core;

export namespace gse::gpu {
	struct specialization_entry {
		std::uint32_t constant_id = 0;
		std::uint32_t offset = 0;
		std::uint32_t size = 0;
	};

	struct shader_object_create_info {
		stage_flag stage = stage_flag::vertex;
		std::span<const std::uint32_t> spirv;
		std::string_view entry_point = "main";
		stage_flags next_stage = {};
		std::optional<std::uint32_t> required_subgroup_size;
		bool require_full_subgroups = false;
		std::span<const specialization_entry> spec_entries;
		std::span<const std::byte> spec_data;
	};

	struct shader_program_create_info {
		std::span<const shader_object_create_info> stages;
		std::span<const binding_use> bindings;
		std::uint32_t push_offset_start = 0;
		std::optional<gpu::push_constant_range> push_constant_range;
		dynamic_pipeline_state state;
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

		shader_program(
			handle<gpu::pipeline_layout> layout,
			std::vector<stage_flag> stages,
			std::vector<handle<gpu::shader_object>> shader_handles,
			dynamic_pipeline_state state,
			bool is_compute,
			bool is_mesh
		);

		[[nodiscard]] auto layout() const -> handle<gpu::pipeline_layout>;

		[[nodiscard]] auto stages() const -> std::span<const stage_flag>;

		[[nodiscard]] auto shader_handles() const -> std::span<const handle<gpu::shader_object>>;

		[[nodiscard]] auto state() const -> const dynamic_pipeline_state&;

		[[nodiscard]] auto is_compute() const -> bool;

		[[nodiscard]] auto is_mesh() const -> bool;

		[[nodiscard]] auto bind_point() const -> gpu::bind_point;

		[[nodiscard]] auto valid() const -> bool;

	private:
		handle<gpu::pipeline_layout> m_layout;
		std::vector<stage_flag> m_stages;
		std::vector<handle<gpu::shader_object>> m_shader_handles;
		dynamic_pipeline_state m_state;
		bool m_is_compute = false;
		bool m_is_mesh = false;
	};
}

gse::gpu::shader_program::shader_program(const handle<gpu::pipeline_layout> layout, std::vector<stage_flag> stages, std::vector<handle<gpu::shader_object>> shader_handles, dynamic_pipeline_state state, const bool is_compute, const bool is_mesh)
	: m_layout(layout), m_stages(std::move(stages)), m_shader_handles(std::move(shader_handles)), m_state(std::move(state)), m_is_compute(is_compute), m_is_mesh(is_mesh) {
}

auto gse::gpu::shader_program::layout() const -> handle<gpu::pipeline_layout> {
	return m_layout;
}

auto gse::gpu::shader_program::stages() const -> std::span<const stage_flag> {
	return m_stages;
}

auto gse::gpu::shader_program::shader_handles() const -> std::span<const handle<gpu::shader_object>> {
	return m_shader_handles;
}

auto gse::gpu::shader_program::state() const -> const dynamic_pipeline_state& {
	return m_state;
}

auto gse::gpu::shader_program::is_compute() const -> bool {
	return m_is_compute;
}

auto gse::gpu::shader_program::is_mesh() const -> bool {
	return m_is_mesh;
}

auto gse::gpu::shader_program::bind_point() const -> gpu::bind_point {
	return m_is_compute ? gpu::bind_point::compute : gpu::bind_point::graphics;
}

auto gse::gpu::shader_program::valid() const -> bool {
	return static_cast<bool>(m_layout);
}
