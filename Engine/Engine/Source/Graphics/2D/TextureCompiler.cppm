export module gse.graphics:texture_compiler;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

import :texture;

export namespace gse {

	auto bake(
		const std::filesystem::path& src,
		texture::baked& out
	) -> bool;
}

namespace gse {

	auto read_texture_profile(
		const std::filesystem::path& src
	) -> texture::profile;
}

auto gse::bake(const std::filesystem::path& src, texture::baked& out) -> bool {
	auto image_data = image::load(src);
	if (image_data.pixels.empty()) {
		log::println(log::level::warning, log::category::assets, "Failed to load texture '{}', skipping", src.generic_display_string());
		return false;
	}

	out.width = image_data.size.x();
	out.height = image_data.size.y();
	out.channels = image_data.channels;
	out.profile = read_texture_profile(src);
	out.pixels.storage = std::move(image_data.pixels);
	return true;
}

auto gse::read_texture_profile(const std::filesystem::path& src) -> texture::profile {
	const auto meta_path = src.parent_path() / (src.stem().native_encoded_string() + ".meta");
	if (!std::filesystem::exists(meta_path)) {
		return texture::profile::generic_repeat;
	}

	std::ifstream meta_file(meta_path);
	std::string line;
	if (!std::getline(meta_file, line) || !line.starts_with("profile:")) {
		return texture::profile::generic_repeat;
	}

	std::string profile_str = line.substr(8);
	profile_str.erase(0, profile_str.find_first_not_of(" \t\r\n"));
	profile_str.erase(profile_str.find_last_not_of(" \t\r\n") + 1);

	if (profile_str == "msdf") {
		return texture::profile::msdf;
	}
	if (profile_str == "pixel_art") {
		return texture::profile::pixel_art;
	}
	if (profile_str == "clamp_to_edge") {
		return texture::profile::generic_clamp_to_edge;
	}
	return texture::profile::generic_repeat;
}
