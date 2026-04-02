export module gse.platform:gpu_descriptor_writer;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_pipeline;
import :gpu_image;
import :gpu_descriptor;
import :descriptor_heap;
import :resource_handle;
import :shader;

import gse.utility;

export namespace gse::gpu {
	class descriptor_writer final : public non_copyable {
	public:
		descriptor_writer(resource::handle<shader> s, descriptor_region& region);
		descriptor_writer(resource::handle<shader> s, vulkan::descriptor_heap& heap);
		descriptor_writer(descriptor_writer&&) noexcept = default;
		auto operator=(descriptor_writer&&) noexcept -> descriptor_writer& = default;

		auto buffer(std::string_view name, const gpu::buffer& buf) -> descriptor_writer&;
		auto buffer(std::string_view name, const gpu::buffer& buf, std::size_t offset, std::size_t range) -> descriptor_writer&;
		auto image(std::string_view name, const gpu::image& img, const gpu::sampler& samp, image_layout layout = image_layout::shader_read_only) -> descriptor_writer&;
		auto image_array(std::string_view name, std::span<const gpu::image* const> images, const gpu::sampler& samp, image_layout layout = image_layout::shader_read_only) -> descriptor_writer&;
		auto image_array(std::string_view name, std::span<const gpu::image* const> images, std::span<const gpu::sampler* const> samplers, image_layout layout = image_layout::shader_read_only) -> descriptor_writer&;

		auto commit() -> void;

		auto begin(std::uint32_t frame_index) -> void;

		[[nodiscard]] auto native_writer(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_push_writer); }
	private:
		enum class mode { persistent, push };

		resource::handle<shader> m_shader;
		descriptor_region* m_region = nullptr;
		mode m_mode = mode::persistent;

		std::unordered_map<std::string, vk::DescriptorBufferInfo> m_buffer_infos;
		std::unordered_map<std::string, vk::DescriptorImageInfo> m_image_infos;
		std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>> m_image_array_infos;

		vulkan::descriptor_writer m_push_writer;
	};
}

namespace {
	auto to_vk_layout(const gse::gpu::image_layout l) -> vk::ImageLayout {
		switch (l) {
			case gse::gpu::image_layout::general:          return vk::ImageLayout::eGeneral;
			case gse::gpu::image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
			default:                                       return vk::ImageLayout::eUndefined;
		}
	}
}

gse::gpu::descriptor_writer::descriptor_writer(resource::handle<shader> s, descriptor_region& region)
	: m_shader(std::move(s)), m_region(&region) {}

gse::gpu::descriptor_writer::descriptor_writer(resource::handle<shader> s, vulkan::descriptor_heap& heap)
	: m_shader(std::move(s)), m_mode(mode::push),
	  m_push_writer(m_shader->create_writer(heap, shader::set::binding_type::push)) {}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const gpu::buffer& buf) -> descriptor_writer& {
	return buffer(name, buf, 0, buf.size());
}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const gpu::buffer& buf, std::size_t offset, std::size_t range) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_buffer_infos[std::string(name)] = vk::DescriptorBufferInfo{
			.buffer = buf.native().buffer,
			.offset = static_cast<vk::DeviceSize>(offset),
			.range = static_cast<vk::DeviceSize>(range)
		};
	}
	else {
		const auto binding_opt = m_shader->binding(std::string(name));
		if (binding_opt) {
			m_push_writer.buffer(binding_opt->binding, buf, static_cast<vk::DeviceSize>(offset), static_cast<vk::DeviceSize>(range));
		}
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image(const std::string_view name, const gpu::image& img, const gpu::sampler& samp, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_image_infos[std::string(name)] = vk::DescriptorImageInfo{
			.sampler = samp.native(),
			.imageView = img.native().view,
			.imageLayout = to_vk_layout(layout)
		};
	}
	else {
		const auto binding_opt = m_shader->binding(std::string(name));
		if (binding_opt) {
			m_push_writer.image(binding_opt->binding, img, samp, layout);
		}
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const gpu::image* const> images, const gpu::sampler& samp, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	for (const auto* img : images) {
		vec.push_back({
			.sampler = samp.native(),
			.imageView = img->native().view,
			.imageLayout = to_vk_layout(layout)
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const gpu::image* const> images, const std::span<const gpu::sampler* const> samplers, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	const auto vk_layout = to_vk_layout(layout);
	for (std::size_t i = 0; i < images.size(); ++i) {
		vec.push_back({
			.sampler = samplers[i]->native(),
			.imageView = images[i]->native().view,
			.imageLayout = vk_layout
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::commit() -> void {
	m_shader->write_descriptors(m_region->native(), m_buffer_infos, m_image_infos, m_image_array_infos);
	m_buffer_infos.clear();
	m_image_infos.clear();
	m_image_array_infos.clear();
}

auto gse::gpu::descriptor_writer::begin(const std::uint32_t frame_index) -> void {
	m_push_writer.begin(frame_index);
}

