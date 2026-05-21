module gse.graphics;

import std;

import :texture;

import gse.assert;
import gse.core;
import gse.config;
import gse.concurrency;
import gse.math;
import gse.os;
import gse.gpu;
import gse.assets;
import gse.log;

gse::texture::texture(const std::filesystem::path& filepath)
	: identifiable(filepath, config::baked_resource_path),
	  m_image_data{
		  .path = filepath
	  } {
}

gse::texture::texture(const std::string_view name, const vec4f& color, const vec2u size)
	: identifiable(name),
	  m_image_data(image::load(color, size)) {
}

gse::texture::texture(
	const std::string_view name,
	const std::vector<std::byte>& data,
	const vec2u size,
	const std::uint32_t channels,
	const profile texture_profile
)
	: identifiable(name),
	  m_image_data(
		  image::data{
			  .path = {},
			  .size = size,
			  .channels = channels,
			  .pixels = data
		  }
	  ),
	  m_profile(texture_profile) {
}

auto gse::texture::load(asset::load_ctx& ctx) -> async::task<> {
	if (!m_image_data.path.empty()) {
		texture::baked baked{};
		if (!load_baked(m_image_data.path, baked)) {
			co_return;
		}

		m_image_data.size = { baked.width, baked.height };
		m_image_data.channels = baked.channels;
		m_image_data.pixels = std::move(baked.pixels.storage);
		m_profile = baked.profile;
	}

	auto& gpu_s = co_await gpu::on_gpu(ctx.channels);
	create_vulkan_resources(gpu_s, m_profile);
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

auto gse::texture::bindless_slot() const -> gpu::bindless_texture_slot {
	return m_bindless_slot;
}

auto gse::texture::upload_token() const -> const gpu::sync_token& {
	return m_upload_token;
}

auto gse::texture::create_vulkan_resources(gpu::context::data& context, const profile texture_profile) -> void {
	const auto width = m_image_data.size.x();
	const auto height = m_image_data.size.y();
	const auto channels = m_image_data.channels;
	const auto data_size = m_image_data.size_bytes();

	assert(
		data_size > 0 && !m_image_data.pixels.empty(),
		"Texture '{}' has no pixel data. Ensure the texture is loaded correctly.",
		id()
	);

	const bool use_linear = (texture_profile == profile::msdf);
	const auto gpu_format = channels == 4
		? (use_linear ? gpu::image_format::r8g8b8a8_unorm : gpu::image_format::r8g8b8a8_srgb)
		: channels == 1 ? gpu::image_format::r8_unorm
						: (use_linear ? gpu::image_format::r8g8b8_unorm : gpu::image_format::r8g8b8_srgb);

	m_image = gpu::image::create(
		context.device->vulkan_device(),
		{
			.size = { width, height },
			.format = gpu_format,
			.usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst,
		},
		std::format("texture:{}", id())
	);

	m_upload_token = gpu::upload_image_2d(*context.device, m_image, m_image_data.pixels.data(), data_size);

	constexpr auto clamp = gpu::sampler_address_mode::clamp_to_edge;
	constexpr auto repeat = gpu::sampler_address_mode::repeat;
	constexpr auto linear = gpu::sampler_filter::linear;
	constexpr auto nearest = gpu::sampler_filter::nearest;

	gpu::sampler_desc desc;
	desc.max_lod = 1.0f;

	switch (texture_profile) {
		case profile::generic_repeat:
			desc.mag = linear;
			desc.min = linear;
			desc.address_u = repeat;
			desc.address_v = repeat;
			desc.address_w = repeat;
			desc.max_anisotropy = 16.0f;
			break;
		case profile::generic_clamp_to_edge:
			desc.mag = linear;
			desc.min = linear;
			desc.address_u = clamp;
			desc.address_v = clamp;
			desc.address_w = clamp;
			break;
		case profile::msdf:
			desc.mag = linear;
			desc.min = linear;
			desc.address_u = clamp;
			desc.address_v = clamp;
			desc.address_w = clamp;
			break;
		case profile::pixel_art:
			desc.mag = nearest;
			desc.min = nearest;
			desc.address_u = clamp;
			desc.address_v = clamp;
			desc.address_w = clamp;
			break;
	}
	m_sampler = gpu::sampler::create(context.device->vulkan_device(), desc);

	m_bindless_slot = context.bindless_textures->allocate(m_image.view(), m_sampler.native());

	m_image_data.pixels.clear();
	m_image_data.pixels.shrink_to_fit();
}
