export module gse.graphics:primitives;

import std;

import :model;

import gse.assets;
import gse.core;
import gse.math;

export namespace gse::primitives {
	struct data {
		resource::handle<model> unit_box;
		resource::handle<model> sphere_lo;
		resource::handle<model> sphere_mid;
		resource::handle<model> sphere_hi;
		resource::handle<model> unit_cylinder;
	};

	auto initialize(
		data& d,
		asset::data& assets
	) -> void;
}
