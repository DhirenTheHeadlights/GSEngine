export module gs:crosshair_system;

import std;
import gse;

export namespace gs {
	struct crosshair_system {
		struct [[= gse::settings::category<"Crosshair">{}]] data {
			[[= gse::settings::describe<"Show the crosshair while in-game.">{}]] bool show = true;

			[[
				= gse::settings::describe<"Length of each arm in pixels.">{},
				= gse::settings::range<0.f, 30.f>{}
			]] float arm_length = 8.f;

			[[
				= gse::settings::describe<"Thickness of each arm in pixels.">{},
				= gse::settings::range<1.f, 8.f>{}
			]] float arm_thickness = 2.f;

			[[
				= gse::settings::describe<"Gap between center and each arm.">{},
				= gse::settings::range<0.f, 20.f>{}
			]] float gap = 3.f;

			[[= gse::settings::describe<"Show the center dot.">{}]] bool show_dot = true;

			[[
				= gse::settings::describe<"Center dot size in pixels.">{},
				= gse::settings::range<1.f, 6.f>{}
			]] float dot_size = 1.f;

			[[
				= gse::settings::describe<"Red channel.">{},
				= gse::settings::range<0.f, 1.f>{}
			]] float color_r = 1.f;

			[[
				= gse::settings::describe<"Green channel.">{},
				= gse::settings::range<0.f, 1.f>{}
			]] float color_g = 1.f;

			[[
				= gse::settings::describe<"Blue channel.">{},
				= gse::settings::range<0.f, 1.f>{}
			]] float color_b = 1.f;

			[[
				= gse::settings::describe<"Opacity.">{},
				= gse::settings::range<0.f, 1.f>{}
			]] float opacity = 0.9f;

			[[
				= gse::settings::describe<"Outline thickness around each arm (0 = no outline).">{},
				= gse::settings::range<0.f, 3.f>{}
			]] float outline_thickness = 0.f;

			[[
				= gse::settings::describe<"Outline opacity.">{},
				= gse::settings::range<0.f, 1.f>{}
			]] float outline_opacity = 1.f;
		};
	};
}
