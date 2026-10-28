export module sandbox:world_catalog;

import std;
import gse;

import :parity_fixtures;
import :world_diagrams;
import :worlds;

export namespace sandbox {
	struct world_group_info {
		char heading[32]{};
	};

	enum class world_group : std::uint8_t {
		world [[= world_group_info{
			.heading = "Worlds",
		}]],
		parity [[= world_group_info{
			.heading = "Parity fixtures",
		}]]
	};

	struct world_entry {
		std::string_view name;
		std::string_view label;
		std::string_view blurb;
		gse::scene::setup_fn setup;
		gse::gui::symbol_glyph diagram = nullptr;
		world_group group = world_group::world;
	};

	[[nodiscard]] auto world_catalog() -> std::span<const world_entry>;
}

auto sandbox::world_catalog() -> std::span<const world_entry> {
	static const std::array<world_entry, 20> entries{
		world_entry{
			.name = "Sandbox",
			.label = "Sandbox",
			.blurb = "Crates, characters and a sidearm to knock them over with",
			.setup = &sandbox_scene_setup,
			.diagram = &diagram::playground,
			.group = world_group::world,
		},
		world_entry{
			.name = "Pyramid",
			.label = "Pyramid",
			.blurb = "A stacked course of boxes the solver has to hold still",
			.setup = &pyramid_scene_setup,
			.diagram = &diagram::pyramid,
			.group = world_group::world,
		},
		world_entry{
			.name = "Tumblers",
			.label = "Tumblers",
			.blurb = "Spinning drums that never let their contents settle",
			.setup = &tumbler_scene_setup,
			.diagram = &diagram::tumbler,
			.group = world_group::world,
		},
		world_entry{
			.name = "Carousel",
			.label = "Carousel",
			.blurb = "Two counter-rotating drums carrying a few thousand cubes",
			.setup = &carousel_scene_setup,
			.diagram = &diagram::carousel,
			.group = world_group::world,
		},
		world_entry{
			.name = "Machines",
			.label = "Machines",
			.blurb = "Lifts, a vise and a swinging door driving loose cargo",
			.setup = &machines_scene_setup,
			.diagram = &diagram::machines,
			.group = world_group::world,
		},
		world_entry{
			.name = "Dominoes",
			.label = "Dominoes",
			.blurb = "Six runs toppling outwards from one hub",
			.setup = &dominoes_scene_setup,
			.diagram = &diagram::domino,
			.group = world_group::world,
		},
		world_entry{
			.name = "Swings",
			.label = "Swings",
			.blurb = "Hanging chains and springs for the joint solver",
			.setup = &swings_scene_setup,
			.diagram = &diagram::swings,
			.group = world_group::world,
		},
		world_entry{
			.name = "Sky",
			.label = "Sky",
			.blurb = "Atmosphere and clouds over an empty grid",
			.setup = &sky_scene_setup,
			.diagram = &diagram::sky,
			.group = world_group::world,
		},
		world_entry{
			.name = "LightHall",
			.label = "Light Hall",
			.blurb = "A corridor of clustered point lights for the culling pass",
			.setup = &light_hall_scene_setup,
			.diagram = &diagram::light_hall,
			.group = world_group::world,
		},
		world_entry{
			.name = "ParityDrop",
			.label = "Drop",
			.blurb = "One box falling onto the ground plane",
			.setup = &parity_drop_scene_setup,
			.diagram = &diagram::drop,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityPair",
			.label = "Pair",
			.blurb = "Two resting boxes sharing one contact",
			.setup = &parity_pair_scene_setup,
			.diagram = &diagram::pair,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityStack",
			.label = "Stack",
			.blurb = "A vertical column, the classic stacking test",
			.setup = &parity_stack_scene_setup,
			.diagram = &diagram::stack,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityCluster",
			.label = "Cluster",
			.blurb = "Bodies packed close enough to share contact islands",
			.setup = parity_grid_fixture({
				.prefix = "ParityCluster",
				.floor_extent = gse::meters(40.f),
				.side = 3,
				.spacing = 0.75f,
			}),
			.diagram = &diagram::cluster,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityHeap",
			.label = "Heap",
			.blurb = "A regular heap that settles into layers",
			.setup = parity_grid_fixture({
				.prefix = "ParityHeap",
				.floor_extent = gse::meters(40.f),
				.side = 4,
				.spacing = 0.75f,
			}),
			.diagram = &diagram::heap,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityMound",
			.label = "Mound",
			.blurb = "A shallow mound that spreads as it relaxes",
			.setup = parity_grid_fixture({
				.prefix = "ParityMound",
				.floor_extent = gse::meters(40.f),
				.side = 5,
				.spacing = 0.75f,
			}),
			.diagram = &diagram::mound,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityPile",
			.label = "Pile",
			.blurb = "An irregular pile with no two contacts alike",
			.setup = parity_grid_fixture({
				.prefix = "ParityPile",
				.floor_extent = gse::meters(40.f),
				.side = 6,
				.spacing = 0.75f,
			}),
			.diagram = &diagram::pile,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityHullPile",
			.label = "Hull Pile",
			.blurb = "The same pile built from convex hulls instead of boxes",
			.setup = parity_grid_fixture({
				.prefix = "ParityHullPile",
				.floor_extent = gse::meters(40.f),
				.side = 6,
				.spacing = 0.75f,
				.hulls = true,
			}),
			.diagram = &diagram::hull_pile,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityOverlap",
			.label = "Overlap",
			.blurb = "Bodies spawned already penetrating, to test recovery",
			.setup = parity_grid_fixture({
				.prefix = "ParityOverlap",
				.floor_extent = gse::meters(40.f),
				.side = 6,
				.spacing = 0.4f,
			}),
			.diagram = &diagram::overlap,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityShapes",
			.label = "Shapes",
			.blurb = "One of every collider shape on the same ground",
			.setup = &parity_shapes_scene_setup,
			.diagram = &diagram::shapes,
			.group = world_group::parity,
		},
		world_entry{
			.name = "ParityDomino",
			.label = "Domino",
			.blurb = "A leaning row that propagates one contact at a time",
			.setup = &parity_domino_scene_setup,
			.diagram = &diagram::domino,
			.group = world_group::parity,
		},
	};
	return entries;
}
