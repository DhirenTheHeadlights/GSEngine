module gse.graphics:image_attachments_impl;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.log;
import gse.math;
import gse.os;
import gse.process;
import gse.time;
import std;

import :image_attachments;
import :texture;
import :types;

auto gse::gui::resolve_image_paste(image_paste_state& paste, const shared_view<asset::data> assets) -> void {
	if (paste.ready || !paste.target.exists()) {
		return;
	}

	std::optional<clipboard::image> pasted = clipboard::take_image();
	if (!pasted) {
		if (!clipboard::image_pending()) {
			paste.target = {};
		}
		return;
	}

	image::data decoded = pasted->path.empty()
		? image::data{
			.size = pasted->size,
			.channels = 4,
			.pixels = std::move(pasted->pixels),
		}
		: image::load_rgba(pasted->path);

	if (decoded.pixels.empty() || decoded.size.x() == 0 || decoded.size.y() == 0) {
		if (pasted->path.empty()) {
			log::println(log::level::warning, log::category::assets, "clipboard bitmap could not be decoded ({})", pasted->size);
		}
		else {
			log::println(log::level::warning, log::category::assets, "clipboard image could not be decoded: {}", pasted->path.display_string());
		}
		paste.target = {};
		return;
	}

	std::filesystem::path path = std::move(pasted->path);
	std::optional<image_encoding> encoding = encoding_for(path);

	if (!encoding) {
		path = process::temporary_path("gui_paste", "png");
		if (!image::write_png(path, decoded.size.x(), decoded.size.y(), 4, decoded.pixels.data())) {
			log::println(log::level::warning, log::category::assets, "could not write pasted image to {}", path.display_string());
			paste.target = {};
			return;
		}
		encoding = image_encoding::png;
	}

	const std::uint32_t index = pasted_image_count++;

	paste.ready = image_attachment{
		.path = std::move(path),
		.size = decoded.size,
		.encoding = *encoding,
		.preview = asset::queue<texture>(
			assets,
			std::format("gui_paste_{}", index),
			decoded.pixels,
			decoded.size,
			4u,
			texture::profile::generic_clamp_to_edge
		),
	};
}
