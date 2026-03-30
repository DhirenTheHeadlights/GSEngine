export module gse.platform:gpu_types;

import std;

import :vulkan_allocator;
import gse.utility;

export namespace gse::gpu {
	enum class buffer_usage : std::uint32_t {
		uniform      = 0x01,
		storage      = 0x02,
		indirect     = 0x04,
		transfer_dst = 0x08,
		vertex       = 0x10,
		index        = 0x20,
	};

	constexpr auto operator|(buffer_usage a, buffer_usage b) -> buffer_usage {
		return static_cast<buffer_usage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
	}

	constexpr auto operator&(buffer_usage a, buffer_usage b) -> buffer_usage {
		return static_cast<buffer_usage>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
	}

	constexpr auto has_flag(buffer_usage flags, buffer_usage flag) -> bool {
		return (flags & flag) == flag;
	}

	struct buffer_desc {
		std::size_t size = 0;
		buffer_usage usage = buffer_usage::storage;
	};

	enum class cull_mode : std::uint8_t { none, front, back };
	enum class compare_op : std::uint8_t { never, less, equal, less_or_equal, greater, not_equal, greater_or_equal, always };
	enum class polygon_mode : std::uint8_t { fill, line, point };
	enum class topology : std::uint8_t { triangle_list, line_list, point_list };

	enum class blend_preset : std::uint8_t {
		none,
		alpha,
		alpha_premultiplied
	};

	struct depth_state {
		bool test = true;
		bool write = true;
		compare_op compare = compare_op::less;
	};

	struct rasterization_state {
		polygon_mode polygon = polygon_mode::fill;
		cull_mode cull = cull_mode::back;
		float line_width = 1.0f;
		bool depth_bias = false;
		float depth_bias_constant = 0.0f;
		float depth_bias_clamp = 0.0f;
		float depth_bias_slope = 0.0f;
	};

	enum class color_format : std::uint8_t { swapchain, none };
	enum class depth_format : std::uint8_t { d32_sfloat, none };

	enum class sampler_filter : std::uint8_t { nearest, linear };
	enum class sampler_address_mode : std::uint8_t { repeat, clamp_to_edge, clamp_to_border, mirrored_repeat };
	enum class border_color : std::uint8_t { float_opaque_white, float_opaque_black, float_transparent_black };

	struct sampler_desc {
		sampler_filter min = sampler_filter::linear;
		sampler_filter mag = sampler_filter::linear;
		sampler_address_mode address_u = sampler_address_mode::repeat;
		sampler_address_mode address_v = sampler_address_mode::repeat;
		sampler_address_mode address_w = sampler_address_mode::repeat;
		bool compare_enable = false;
		compare_op compare = compare_op::always;
		border_color border = border_color::float_opaque_white;
		float max_anisotropy = 0.0f;
		float min_lod = 0.0f;
		float max_lod = 0.0f;
	};

	class buffer final : public non_copyable {
	public:
		buffer() = default;
		buffer(vulkan::buffer_resource&& resource);
		buffer(buffer&&) noexcept = default;
		auto operator=(buffer&&) noexcept -> buffer& = default;

		[[nodiscard]] auto mapped() const -> std::byte*;
		[[nodiscard]] auto size() const -> std::size_t;

		[[nodiscard]] auto native() const -> const vulkan::buffer_resource&;
		[[nodiscard]] auto native() -> vulkan::buffer_resource&;

		explicit operator bool() const;
	private:
		vulkan::buffer_resource m_resource;
	};

	class pipeline final : public non_copyable {
	public:
		pipeline() = default;
		pipeline(
			vk::raii::Pipeline&& handle,
			vk::raii::PipelineLayout&& layout
		);
		pipeline(pipeline&&) noexcept = default;
		auto operator=(pipeline&&) noexcept -> pipeline& = default;

		[[nodiscard]] auto native_pipeline() const -> vk::Pipeline;
		[[nodiscard]] auto native_layout() const -> vk::PipelineLayout;

		explicit operator bool() const;
	private:
		vk::raii::Pipeline m_handle = nullptr;
		vk::raii::PipelineLayout m_layout = nullptr;
	};

	class descriptor_set final : public non_copyable {
	public:
		descriptor_set() = default;
		descriptor_set(vk::raii::DescriptorSet&& set);
		descriptor_set(descriptor_set&&) noexcept = default;
		auto operator=(descriptor_set&&) noexcept -> descriptor_set& = default;

		[[nodiscard]] auto native() const -> vk::DescriptorSet;

		explicit operator bool() const;
	private:
		vk::raii::DescriptorSet m_set = nullptr;
	};

	class sampler final : public non_copyable {
	public:
		sampler() = default;
		sampler(vk::raii::Sampler&& s);
		sampler(sampler&&) noexcept = default;
		auto operator=(sampler&&) noexcept -> sampler& = default;

		[[nodiscard]] auto native() const -> vk::Sampler;

		explicit operator bool() const;
	private:
		vk::raii::Sampler m_sampler = nullptr;
	};

	struct descriptor_buffer_binding {
		std::uint32_t binding = 0;
		const buffer* buf = nullptr;
		std::size_t offset = 0;
		std::size_t range = 0;
	};
}

gse::gpu::buffer::buffer(vulkan::buffer_resource&& resource)
	: m_resource(std::move(resource)) {}

auto gse::gpu::buffer::mapped() const -> std::byte* {
	return m_resource.allocation.mapped();
}

auto gse::gpu::buffer::size() const -> std::size_t {
	return m_resource.allocation.size();
}

auto gse::gpu::buffer::native() const -> const vulkan::buffer_resource& {
	return m_resource;
}

auto gse::gpu::buffer::native() -> vulkan::buffer_resource& {
	return m_resource;
}

gse::gpu::buffer::operator bool() const {
	return m_resource.buffer != nullptr;
}

gse::gpu::pipeline::pipeline(
	vk::raii::Pipeline&& handle,
	vk::raii::PipelineLayout&& layout
) : m_handle(std::move(handle)),
    m_layout(std::move(layout)) {}

auto gse::gpu::pipeline::native_pipeline() const -> vk::Pipeline {
	return *m_handle;
}

auto gse::gpu::pipeline::native_layout() const -> vk::PipelineLayout {
	return *m_layout;
}

gse::gpu::pipeline::operator bool() const {
	return *m_handle != nullptr;
}

gse::gpu::descriptor_set::descriptor_set(vk::raii::DescriptorSet&& set)
	: m_set(std::move(set)) {}

auto gse::gpu::descriptor_set::native() const -> vk::DescriptorSet {
	return *m_set;
}

gse::gpu::descriptor_set::operator bool() const {
	return *m_set != nullptr;
}

gse::gpu::sampler::sampler(vk::raii::Sampler&& s)
	: m_sampler(std::move(s)) {}

auto gse::gpu::sampler::native() const -> vk::Sampler {
	return *m_sampler;
}

gse::gpu::sampler::operator bool() const {
	return *m_sampler != nullptr;
}
