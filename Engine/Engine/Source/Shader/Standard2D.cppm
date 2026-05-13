export module gse.shader:standard_2d;

import std;

import gse.containers;
import gse.gpu;

export namespace gse::shaders::standard_2d {
	struct [[= binding<2, 0>{}, = sampler2d_array]] g_textures {};

	using shader_binding_types = type_pack<g_textures>;
}
