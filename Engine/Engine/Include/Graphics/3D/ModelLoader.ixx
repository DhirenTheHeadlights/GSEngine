export module gse.graphics:model_loader;

import std;

import :model;
import :mesh;
import :texture;
import :texture_loader;
import :material;

import gse.physics.math;
import gse.utility;
import gse.platform;

export namespace gse {
	using model_loader = resource_loader<model, model::handle, renderer::context>;
}