export module gse.gpu:image;

import std;

import :gpu_task;
import :sync_token;
import :device;

import gse.gpu_backend;
import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;

export namespace gse::gpu {
	auto transition_image_to(
		device& dev,
		image& img
	) -> sync_token;
}