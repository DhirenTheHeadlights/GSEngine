export module gse.graphics:texture;

import std;

import gse.core;
import gse.math;
import gse.os;
import gse.gpu;
import gse.assets;
import gse.containers;
import gse.concurrency;

export namespace gse {
	class texture : public identifiable {
	public:
		enum struct profile : std::uint8_t {
			generic_repeat,
			generic_clamp_to_edge,
			msdf,
			pixel_art
		};

		struct [[
			= asset_format::baked_ext<".gtx">{},
			= asset_format::baked_dir<"Textures">{},
			= asset_format::source_exts<".png", ".jpg", ".jpeg", ".tga", ".bmp">{},
			= asset_format::magic<0x47544558>{},
			= asset_format::version<1>{},
			= asset_format::meta_sidecar<>{}
		]] baked {
			std::uint32_t width = 0;
			std::uint32_t height = 0;
			std::uint32_t channels = 0;
			profile profile = profile::generic_repeat;
			raw_blob_owned<std::byte> pixels;
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
			asset::load_ctx& ctx
		) -> async::task<>;

		auto unload() -> void;

		auto gpu_image() const -> const gpu::image&;

		auto image_data() const -> const image::data&;

		[[nodiscard]] auto bindless_slot() const -> gpu::bindless_slot;

		auto upload_token() const -> const gpu::sync_token&;

	private:
		auto create_vulkan_resources(
			gpu::context::data& context,
			profile texture_profile
		) -> void;

		gpu::image m_image;
		gpu::bindless_handle m_bindless_slot;
		image::data m_image_data;
		profile m_profile = profile::generic_repeat;
		gpu::sync_token m_upload_token;
	};
}
