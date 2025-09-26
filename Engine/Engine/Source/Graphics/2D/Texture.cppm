export module gse.graphics:texture;

import std;

import :rendering_context;

import gse.utility;
import gse.physics.math;
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
			const unitless::vec4& color, 
			unitless::vec2u size = { 1, 1 }
		);

		texture(
			std::string_view name,
			const std::vector<std::byte>& data,
			unitless::vec2u size,
			std::uint32_t channels,
			profile texture_profile = profile::generic_repeat
		);

		static auto compile(
		) -> std::set<std::filesystem::path>;

		auto load(
			const renderer::context& context
		) -> void;

		auto unload(
		) -> void;

		auto descriptor_info(
		) const -> vk::DescriptorImageInfo;

		auto image_data(
		) const -> const image::data&;
	private:
		auto create_vulkan_resources(
			renderer::context& context,
			profile texture_profile
		) -> void;

		vulkan::persistent_allocator::image_resource m_texture_image;
		vk::raii::Sampler m_texture_sampler = nullptr;
		image::data m_image_data;
		profile m_profile = profile::generic_repeat;
	};
}

gse::texture::texture(const std::filesystem::path& filepath) : identifiable(filepath), m_image_data{ .path = filepath } {}

gse::texture::texture(const std::string_view name, const unitless::vec4& color, const unitless::vec2u size) : identifiable(name), m_image_data(image::load(color, size)) {}

gse::texture::texture(const std::string_view name, const std::vector<std::byte>& data, const unitless::vec2u size, const std::uint32_t channels, const profile texture_profile)
	: identifiable(name), m_image_data(image::data{ .path = {}, .size = size, .channels = channels, .pixels = data }), m_profile(texture_profile) {}

auto gse::texture::compile() -> std::set<std::filesystem::path> {
	const auto source_root = config::resource_path;
	const auto baked_root = config::baked_resource_path / "Textures";

	if (!exists(source_root)) return {};
	if (!exists(baked_root)) {
		create_directories(baked_root);
	}

	std::println("Compiling textures...");

	const std::vector<std::string> supported_extensions = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };

	std::set<std::filesystem::path> resources;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
		if (!entry.is_regular_file()) continue;

		if (const auto extension = entry.path().extension().string(); std::ranges::find(supported_extensions, extension) == supported_extensions.end()) {
			continue;
		}

		const auto source_path = entry.path();
		auto relative_path = source_path.lexically_relative(source_root);
		const auto baked_path = baked_root / relative_path.replace_extension(".gtx");
		const auto meta_path = source_path.parent_path() / (source_path.stem().string() + ".meta");
		resources.insert(baked_path);

		bool needs_recompile = !exists(baked_path);
		if (!needs_recompile) {
			if (const auto dst_time = last_write_time(baked_path); last_write_time(source_path) > dst_time) {
				needs_recompile = true;
			}
			else if (exists(meta_path) && last_write_time(meta_path) > dst_time) {
				needs_recompile = true;
			}
		}

		if (!needs_recompile) continue;

		const auto image_data = image::load(source_path);
		if (image_data.pixels.empty()) {
			std::println("Warning: Failed to load texture '{}', skipping.", source_path.string());
			continue;
		}

		auto texture_profile = profile::generic_repeat;
		if (exists(meta_path)) {
			std::ifstream meta_file(meta_path);
			if (std::string line; std::getline(meta_file, line) && line.starts_with("profile:")) {
				std::string profile_str = line.substr(8);
				profile_str.erase(0, profile_str.find_first_not_of(" \t\r\n"));
				profile_str.erase(profile_str.find_last_not_of(" \t\r\n") + 1);

				if (profile_str == "msdf") texture_profile = profile::msdf;
				else if (profile_str == "pixel_art") texture_profile = profile::pixel_art;
				else if (profile_str == "clamp_to_edge") texture_profile = profile::generic_clamp_to_edge;
			}
		}

		create_directories(baked_path.parent_path());
		std::ofstream out_file(baked_path, std::ios::binary);
		assert(out_file.is_open(), "Failed to open baked texture file for writing.");

		constexpr std::uint32_t magic = 0x47544558; // 'GTEX'
		constexpr std::uint32_t version = 1;
		out_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
		out_file.write(reinterpret_cast<const char*>(&version), sizeof(version));

		const std::uint32_t width = image_data.size.x();
		const std::uint32_t height = image_data.size.y();
		const std::uint32_t channels = image_data.channels;

		out_file.write(reinterpret_cast<const char*>(&width), sizeof(width));
		out_file.write(reinterpret_cast<const char*>(&height), sizeof(height));
		out_file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
		out_file.write(reinterpret_cast<const char*>(&texture_profile), sizeof(texture_profile));

		const std::uint64_t data_size = image_data.size_bytes();
		out_file.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
		out_file.write(reinterpret_cast<const char*>(image_data.pixels.data()), data_size);

		out_file.close();
		std::print("Texture compiled: {}\n", baked_path.filename().string());
	}

	return resources;
}

auto gse::texture::load(const renderer::context& context) -> void {
	if (!m_image_data.path.empty()) {
		std::ifstream in_file(m_image_data.path, std::ios::binary);
		assert(in_file.is_open(), std::format(
			"Failed to open baked texture file: {}",
			m_image_data.path.string()
		));

		std::uint32_t magic, version;
		in_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		in_file.read(reinterpret_cast<char*>(&version), sizeof(version));
		assert(magic == 0x47544558 && version == 1, "Invalid baked texture file format or version.");

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

	context.queue_gpu_command<texture>(this, [](renderer::context& ctx, texture& self) {
		self.create_vulkan_resources(ctx, self.m_profile);
	});
}

auto gse::texture::unload() -> void {
	m_image_data = {};
	m_texture_image = {};
	m_texture_sampler = nullptr;
}

auto gse::texture::descriptor_info() const -> vk::DescriptorImageInfo {
	return {
		.sampler = m_texture_sampler,
		.imageView = m_texture_image.view,
		.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
	};
}

auto gse::texture::image_data() const -> const image::data& {
	return m_image_data;
}

auto gse::texture::create_vulkan_resources(renderer::context& context, const profile texture_profile) -> void {
	auto& config = context.config();

	const auto width = m_image_data.size.x();
	const auto height = m_image_data.size.y();
	const auto channels = m_image_data.channels;
	const auto data_size = m_image_data.size_bytes();

	assert(
		data_size > 0 && !m_image_data.pixels.empty(), 
		std::format(
			"Texture '{}' has no pixel data. Ensure the texture is loaded correctly.",
			id()
		)
	);

	const auto format = channels == 4 ? vk::Format::eR8G8B8A8Srgb
		: channels == 1 ? vk::Format::eR8Unorm
		: vk::Format::eR8G8B8Srgb;

	m_texture_image = vulkan::persistent_allocator::create_image(
		config.device_config(),
		vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {width, height, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			.sharingMode = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined
		},
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		vk::ImageViewCreateInfo{
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		}
	);

	vulkan::uploader::upload_image_2d(
		config,
		m_texture_image,
		width, height,
		m_image_data.pixels.data(),
		data_size,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);

	vk::SamplerCreateInfo sampler_info;
	sampler_info.maxLod = 1.0f;

	switch (texture_profile) {
	case profile::generic_repeat:
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
		sampler_info.anisotropyEnable = vk::True;
		sampler_info.maxAnisotropy = 16.0f;
		break;
	case profile::generic_clamp_to_edge:
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		break;
	case profile::msdf:
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		break;
	case profile::pixel_art:
		sampler_info.magFilter = vk::Filter::eNearest;
		sampler_info.minFilter = vk::Filter::eNearest;
		sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		break;
	}
	m_texture_sampler = config.device_config().device.createSampler(sampler_info);

	m_image_data.pixels.clear();
	m_image_data.pixels.shrink_to_fit();
}
