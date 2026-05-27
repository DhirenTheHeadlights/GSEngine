export module gse.graphics:render_targets;

import gse.gpu;

export namespace gse::renderer::targets {
	struct hdr_color {
		static constexpr gpu::framebuffer_image_desc desc{
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::color_attachment | gpu::image_flag::sampled,
			.aspects = gpu::image_aspect_flag::color,
		};
	};

	struct velocity {
		static constexpr gpu::framebuffer_image_desc desc{
			.format = gpu::image_format::r16g16_sfloat,
			.usage = gpu::image_flag::color_attachment | gpu::image_flag::sampled,
			.aspects = gpu::image_aspect_flag::color,
		};
	};

	struct post_taa_color {
		static constexpr gpu::framebuffer_image_desc desc{
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::color_attachment | gpu::image_flag::sampled,
			.aspects = gpu::image_aspect_flag::color,
		};
	};
}
