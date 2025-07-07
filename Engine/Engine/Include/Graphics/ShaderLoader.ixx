export module gse.graphics:shader_loader;

import std;

import :shader;
import :resource_loader;

import gse.platform;
import gse.utility;

export namespace gse {
	using shader_loader = resource_loader<shader, shader::handle, renderer::context>;
}
