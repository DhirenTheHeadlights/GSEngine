export module gse.physics:muscle_component;


import gse.ecs;

export namespace gse::physics {
	struct muscle_component {
		[[= networked]] float activation = 0.f;
	};
}