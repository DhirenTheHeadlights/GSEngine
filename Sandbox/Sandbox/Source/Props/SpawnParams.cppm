export module sandbox:spawn_params;

import gse;

export namespace sandbox {
	struct stress_scene_params {
		[[
			= gse::settings::describe<"Cubes along each radial axis of a tumbler drum. The drum geometry scales with "
									  "the cube counts at a fixed pitch, so this is the scene's main size dial; the "
									  "two drums hold the bulk of the stress scene's bodies. At the slider maxima the "
									  "pair approaches the solver's body capacity.">{},
			= gse::settings::range<1, 16>{}
		]]
		int tumbler_radial_cubes = 12;

		[[
			= gse::settings::describe<"Cubes along the axial length of a tumbler drum. The drum length scales to "
									  "match.">{},
			= gse::settings::range<1, 32>{}
		]]
		int tumbler_axial_cubes = 24;

		[[
			= gse::settings::describe<"Boxes per side of the stacked box grid.">{},
			= gse::settings::range<1, 16>{}
		]]
		int box_grid_side = 6;

		[[
			= gse::settings::describe<"Layers in the stacked box grid.">{},
			= gse::settings::range<1, 8>{}
		]]
		int box_grid_layers = 3;
	};

	struct character_spawn_params {
		[[
			= gse::settings::describe<"Characters built by one spawn request, whether it came from F7 or the locomotion "
									  "scenario. F7 wants the one character it possesses, so the locomotion scenario pins "
									  "its own higher count instead: a single character measures close to the noise floor, "
									  "and a regression threshold needs several before it means anything.">{},
			= gse::settings::range<1, 32>{}
		]]
		int count = 1;
	};

	struct pyramid_scene_params {
		[[
			= gse::settings::describe<"Blocks along the base of the pyramid workload. The stack is this many rows tall and "
									  "holds base_count * (base_count + 1) / 2 blocks, so this is the dial that sets how "
									  "much load the bottom contacts carry.">{},
			= gse::settings::range<1, 200>{}
		]]
		int base_count = 50;
	};

	struct pyramid_strike_params {
		[[
			= gse::settings::describe<"First pyramid row the strike removes. Row 0 is the base; striking the base does "
									  "not collapse a pyramid, it just drops onto the floor intact.">{},
			= gse::settings::range<0, 200>{}
		]]
		int row_begin = 26;

		[[
			= gse::settings::describe<"How many rows the strike removes.">{},
			= gse::settings::range<1, 40>{}
		]]
		int rows = 6;

		[[
			= gse::settings::describe<"Fraction of each struck row to hit, measured from the left edge. Below 1.0 the "
									  "support is removed from one side only, so the section above topples instead of "
									  "dropping flat.">{},
			= gse::settings::range<0.f, 1.f>{}
		]]
		float width_fraction = 0.5f;

		[[
			= gse::settings::describe<"Impulse applied to each struck block.">{}
		]]
		gse::impulse strength = gse::newton_seconds(500.f);

		[[
			= gse::settings::describe<"Direction the struck blocks are pushed, in world space. Normalized before use.">{}
		]]
		gse::vec3f direction = { -1.f, 0.f, 0.f };
	};

	struct light_field_params {
		[[
			= gse::settings::describe<"How many clusters of point lights a light-field spawn builds. Clustering is what "
									  "loads the tiled light culling pass unevenly, so neighbouring tiles hold wildly "
									  "different light counts instead of all holding the same few.">{},
			= gse::settings::range<1, 4096>{}
		]]
		int clusters = 384;

		[[
			= gse::settings::describe<"How many point lights sit in each cluster. One light per cluster scatters the "
									  "field evenly; raising this concentrates the same budget into pockets.">{},
			= gse::settings::range<1, 64>{}
		]]
		int lights_per_cluster = 1;

		[[
			= gse::settings::describe<"Radius of the cylinder the clusters are scattered through.">{}
		]]
		gse::length field_radius = gse::meters(46.f);

		[[
			= gse::settings::describe<"Height of the cylinder the clusters are scattered through.">{}
		]]
		gse::length field_height = gse::meters(34.f);

		[[
			= gse::settings::describe<"Radius of the sphere each cluster's lights are scattered through.">{}
		]]
		gse::length cluster_radius = gse::meters(5.f);

		[[
			= gse::settings::describe<"Radius of each emissive sphere standing in for a light.">{}
		]]
		gse::length source_radius = gse::meters(0.35f);

		[[
			= gse::settings::describe<"Radiant intensity of each light.">{}
		]]
		gse::irradiance intensity = gse::watts_per_square_meter(26.f);

		[[
			= gse::settings::describe<"How far each light reaches before the culling pass cuts it off. Attenuation is "
									  "solved back from this so the radius holds as intensity changes. This is the dial "
									  "that must come down as density goes up: lights that all reach across the whole "
									  "scene push every tile past the per-tile light cap, and the culling result then "
									  "degrades into visible 16-pixel blocks.">{}
		]]
		gse::length falloff_radius = gse::meters(25.f);

		[[
			= gse::settings::describe<"How far each light drifts from where it spawned. Zero leaves the field static; "
									  "any larger value makes the lights kinematic so the culling pass has to re-derive "
									  "its tile lists every frame.">{}
		]]
		gse::length drift = gse::meters(0.f);

		[[
			= gse::settings::describe<"How fast each light travels around its drift cycle.">{}
		]]
		gse::angular_velocity drift_rate = gse::degrees_per_second(48.f);

		[[
			= gse::settings::describe<"Sun elevation held while the light hall is the active scene. The hall is a night "
									  "scene, so this sits below the horizon and the field is what lights it.">{},
			= gse::settings::range<-90.f, 90.f>{}
		]]
		gse::angle sun_elevation = gse::degrees(-9.f);
	};
}
