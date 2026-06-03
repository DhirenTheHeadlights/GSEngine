export module gse.dx12:pipeline;

import std;

import gse.gpu_types;
import gse.core;

export namespace gse::dx12 {
	struct pipeline_layout {};

	class shader_object final : public non_copyable {
	public:
		shader_object() {}
		~shader_object() = default;

		shader_object(
			shader_object&&
		) noexcept = default;

		auto operator=(
			shader_object&&
		) noexcept -> shader_object& = default;

		[[nodiscard]] auto handle() const -> gpu::shader_object_handle;

		[[nodiscard]] auto stage() const -> gpu::stage_flag;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::shader_object_handle m_handle;
		gpu::stage_flag m_stage = gpu::stage_flag::vertex;
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

		[[nodiscard]] auto layout() const -> gpu::pipeline_layout_handle;

		[[nodiscard]] auto stages() const -> std::span<const gpu::stage_flag>;

		[[nodiscard]] auto shader_handles() const -> std::span<const gpu::shader_object_handle>;

		[[nodiscard]] auto state() const -> const gpu::dynamic_pipeline_state&;

		[[nodiscard]] auto is_compute() const -> bool;

		[[nodiscard]] auto is_mesh() const -> bool;

		[[nodiscard]] auto bind_point() const -> gpu::bind_point;

		[[nodiscard]] auto valid() const -> bool;

	private:
		gpu::pipeline_layout_handle m_layout;
		std::vector<gpu::stage_flag> m_stages;
		std::vector<gpu::shader_object_handle> m_shader_handles;
		gpu::dynamic_pipeline_state m_state;
		bool m_is_compute = false;
		bool m_is_mesh = false;
	};
}

auto gse::dx12::shader_object::handle() const -> gpu::shader_object_handle {
	return m_handle;
}

auto gse::dx12::shader_object::stage() const -> gpu::stage_flag {
	return m_stage;
}

auto gse::dx12::shader_object::valid() const -> bool {
	return false;
}

auto gse::dx12::shader_program::layout() const -> gpu::pipeline_layout_handle {
	return m_layout;
}

auto gse::dx12::shader_program::stages() const -> std::span<const gpu::stage_flag> {
	return m_stages;
}

auto gse::dx12::shader_program::shader_handles() const -> std::span<const gpu::shader_object_handle> {
	return m_shader_handles;
}

auto gse::dx12::shader_program::state() const -> const gpu::dynamic_pipeline_state& {
	return m_state;
}

auto gse::dx12::shader_program::is_compute() const -> bool {
	return m_is_compute;
}

auto gse::dx12::shader_program::is_mesh() const -> bool {
	return m_is_mesh;
}

auto gse::dx12::shader_program::bind_point() const -> gpu::bind_point {
	return m_is_compute ? gpu::bind_point::compute : gpu::bind_point::graphics;
}

auto gse::dx12::shader_program::valid() const -> bool {
	return false;
}
