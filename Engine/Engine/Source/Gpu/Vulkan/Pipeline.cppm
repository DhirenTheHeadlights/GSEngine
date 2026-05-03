export module gse.gpu:vulkan_pipeline;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_descriptor_set_layout;
import :vulkan_device;
import :vulkan_pipeline_layout;

import gse.core;

export namespace gse::vulkan {
	struct graphics_pipeline_create_info {
		std::span<const gpu::shader_stage_create_info> stages;
		std::span<const gpu::vertex_binding_desc> vertex_bindings;
		std::span<const gpu::vertex_attribute_desc> vertex_attributes;
		std::span<const gpu::handle<descriptor_set_layout>> set_layouts;
		std::optional<gpu::push_constant_range> push_constant_range;
		gpu::rasterization_state rasterization;
		gpu::depth_state depth;
		gpu::blend_preset blend = gpu::blend_preset::none;
		gpu::topology topology = gpu::topology::triangle_list;
		gpu::image_format color_format = gpu::image_format::r8g8b8a8_unorm;
		gpu::image_format depth_format = gpu::image_format::d32_sfloat;
		bool has_color = false;
		bool has_depth = false;
		bool is_mesh_pipeline = false;
	};

	struct compute_pipeline_create_info {
		gpu::shader_stage_create_info stage;
		std::span<const gpu::handle<descriptor_set_layout>> set_layouts;
		std::optional<gpu::push_constant_range> push_constant_range;
	};

	class pipeline final : public non_copyable {
	public:
		pipeline() = default;

		~pipeline() override = default;

		pipeline(
			pipeline&&
		) noexcept = default;

		auto operator=(
			pipeline&&
		) noexcept -> pipeline& = default;

		[[nodiscard]] static auto create_graphics(
			const device& dev,
			const graphics_pipeline_create_info& info
		) -> pipeline;

		[[nodiscard]] static auto create_compute(
			const device& dev,
			const compute_pipeline_create_info& info
		) -> pipeline;

		[[nodiscard]] auto handle(
			this const pipeline& self
		) -> gpu::handle<pipeline>;

		[[nodiscard]] auto layout(
			this const pipeline& self
		) -> gpu::handle<pipeline_layout>;

		[[nodiscard]] auto bind_point(
			this const pipeline& self
		) -> gpu::bind_point;

		explicit operator bool(
		) const;

	private:
		pipeline(
			vk::raii::Pipeline&& pipeline,
			vk::raii::PipelineLayout&& layout,
			vk::PipelineBindPoint bind_point
		);

		vk::raii::Pipeline m_pipeline = nullptr;
		vk::raii::PipelineLayout m_layout = nullptr;
		vk::PipelineBindPoint m_bind_point = vk::PipelineBindPoint::eGraphics;
	};
}
