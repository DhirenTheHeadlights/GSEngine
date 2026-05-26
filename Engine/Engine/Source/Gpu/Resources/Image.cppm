export module gse.gpu:image;

import std;

import :aliases;
import :gpu_task;
import :sync_token;
import :transient_api;
import :device;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;

export namespace gse::gpu {
	auto transition_image_to(
		gpu::device& dev,
		image& img
	) -> sync_token;

	auto upload_image_2d(
		gpu::device& dev,
		image& img,
		const void* pixel_data
	) -> sync_token;
}
