export module gse.shader:post_process;

import std;

import gse.containers;
import gse.gpu;

export namespace gse::shaders::post_process {
	struct [[= binding<0, 0>{}, = sampler2d]] input_tex_0 {};
	struct [[= binding<0, 1>{}, = sampler2d]] input_tex_1 {};

	using shader_binding_types = type_pack<input_tex_0, input_tex_1>;
}
