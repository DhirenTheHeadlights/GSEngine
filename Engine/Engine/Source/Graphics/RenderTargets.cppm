export module gse.graphics:render_targets;

import gse.gpu;

export namespace gse::renderer::targets {
	struct hdr_color {
		static constexpr gpu::framebuffer_image_desc desc{
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::color_attachment | gpu::image_flag::sampled,
			.aspects = gpu::image_aspect_flag::color,
			.steady_layout = gpu::image_layout::general,
			.steady_stages =
				gpu::pipeline_stage_flag::color_attachment_output | gpu::pipeline_stage_flag::fragment_shader,
			.steady_access = gpu::access_flag::color_attachment_write | gpu::access_flag::color_attachment_read |
				gpu::access_flag::shader_sampled_read,
		};
	};
}
