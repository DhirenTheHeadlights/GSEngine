export module gse.gpu:image;

import std;

import :aliases;
import :gpu_task;
import :sync_token;
import :device;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;

export namespace gse::gpu {
	[[nodiscard]]
	auto image_aspect_for(
		image_format_value f
	) -> image_aspect_flags;

	[[nodiscard]]
	auto format_value(
		image_format f
	) -> image_format_value;

	auto transition_image_to(
		gpu::device& dev,
		image& img
	) -> sync_token;
}
