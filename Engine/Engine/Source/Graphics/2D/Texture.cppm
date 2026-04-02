export module gse.graphics:texture;

import std;

import gse.assert;
import gse.utility;
import gse.math;
import gse.platform;

export namespace gse {
	class texture : public identifiable {
	public:
		enum struct profile : std::uint8_t {
			generic_repeat,
			generic_clamp_to_edge,
			msdf,
			pixel_art
		};

		texture(
			const std::filesystem::path& filepath
		);

		texture(
			std::string_view name, 
			const vec4f& color, 
			vec2u size = { 1, 1 }
		);

		texture(
			std::string_view name,
			const std::vector<std::byte>& data,
			vec2u size,
			std::uint32_t channels,
			profile texture_profile = profile::generic_repeat
		);

		auto load(
			const gpu::context& context
		) -> void;

		auto unload(
		) -> void;

		auto gpu_image(
		) const -> const gpu::image&;

		auto gpu_sampler(
		) const -> const gpu::sampler&;

		auto image_data(
		) const -> const image::data&;
	private:
		auto create_vulkan_resources(
			gpu::context& context,
			profile texture_profile
		) -> void;

		gpu::image m_image;
		gpu::sampler m_sampler;
		image::data m_image_data;
		profile m_profile = profile::generic_repeat;
	};
}

gse::texture::texture(const std::filesystem::path& filepath) : identifiable(filepath, config::baked_resource_path), m_image_data{ .path = filepath } {}

gse::texture::texture(const std::string_view name, const vec4f& color, const vec2u size) : identifiable(name), m_image_data(image::load(color, size)) {}

gse::texture::texture(const std::string_view name, const std::vector<std::byte>& data, const vec2u size, const std::uint32_t channels, const profile texture_profile) : identifiable(name), m_image_data(image::data{ .path = {}, .size = size, .channels = channels, .pixels = data }), m_profile(texture_profile) {}

auto gse::texture::load(const gpu::context& context) -> void {
	if (!m_image_data.path.empty()) {
		std::ifstream in_file(m_image_data.path, std::ios::binary);
		assert(
			in_file.is_open(), 
			std::source_location::current(),
			"Failed to open baked texture file: {}",
			m_image_data.path.string()
		);

		std::uint32_t magic, version;
		in_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		in_file.read(reinterpret_cast<char*>(&version), sizeof(version));
		assert(magic == 0x47544558 && version == 1, std::source_location::current(), "Invalid baked texture file format or version.");

		std::uint32_t width, height, channels;
		profile texture_profile;
		in_file.read(reinterpret_cast<char*>(&width), sizeof(width));
		in_file.read(reinterpret_cast<char*>(&height), sizeof(height));
		in_file.read(reinterpret_cast<char*>(&channels), sizeof(channels));
		in_file.read(reinterpret_cast<char*>(&texture_profile), sizeof(texture_profile));

		std::uint64_t data_size;
		in_file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
		m_image_data.pixels.resize(data_size);
		in_file.read(reinterpret_cast<char*>(m_image_data.pixels.data()), data_size);

		m_image_data.size = { width, height };
		m_image_data.channels = channels;
	}

	context.queue_gpu_command<texture>(this, [](gpu::context& ctx, texture& self) {
		self.create_vulkan_resources(ctx, self.m_profile);
	});
}

auto gse::texture::unload() -> void {
	m_image_data = {};
	m_image = {};
	m_sampler = {};
}

auto gse::texture::gpu_image() const -> const gpu::image& {
	return m_image;
}

auto gse::texture::gpu_sampler() const -> const gpu::sampler& {
	return m_sampler;
}

auto gse::texture::image_data() const -> const image::data& {
	return m_image_data;
}

auto gse::texture::create_vulkan_resources(gpu::context& context, const profile texture_profile) -> void {
	const auto width = m_image_data.size.x();
	const auto height = m_image_data.size.y();
	const auto channels = m_image_data.channels;
	const auto data_size = m_image_data.size_bytes();

	assert(
		data_size > 0 && !m_image_data.pixels.empty(),
		std::source_location::current(),
		"Texture '{}' has no pixel data. Ensure the texture is loaded correctly.",
		id()
	);

	const bool use_linear = (texture_profile == profile::msdf);
	const auto gpu_format = channels == 4
		? (use_linear ? gpu::image_format::r8g8b8a8_unorm : gpu::image_format::r8g8b8a8_srgb)
		: channels == 1
			? gpu::image_format::r8_unorm
			: (use_linear ? gpu::image_format::r8g8b8_unorm : gpu::image_format::r8g8b8_srgb);

	m_image = gpu::create_image(context.device_ref(), {
		.size = { width, height },
		.format = gpu_format,
		.usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst
	});

	gpu::upload_image_2d(context.device_ref(), m_image, m_image_data.pixels.data(), data_size);

	const auto clamp = gpu::sampler_address_mode::clamp_to_edge;
	const auto repeat = gpu::sampler_address_mode::repeat;
	const auto linear = gpu::sampler_filter::linear;
	const auto nearest = gpu::sampler_filter::nearest;

	gpu::sampler_desc desc;
	desc.max_lod = 1.0f;

	switch (texture_profile) {
	case profile::generic_repeat:
		desc.mag = linear; desc.min = linear;
		desc.address_u = repeat; desc.address_v = repeat; desc.address_w = repeat;
		desc.max_anisotropy = 16.0f;
		break;
	case profile::generic_clamp_to_edge:
		desc.mag = linear; desc.min = linear;
		desc.address_u = clamp; desc.address_v = clamp; desc.address_w = clamp;
		break;
	case profile::msdf:
		desc.mag = linear; desc.min = linear;
		desc.address_u = clamp; desc.address_v = clamp; desc.address_w = clamp;
		break;
	case profile::pixel_art:
		desc.mag = nearest; desc.min = nearest;
		desc.address_u = clamp; desc.address_v = clamp; desc.address_w = clamp;
		break;
	}
	m_sampler = gpu::create_sampler(context.device_ref(), desc);

	m_image_data.pixels.clear();
	m_image_data.pixels.shrink_to_fit();
}
