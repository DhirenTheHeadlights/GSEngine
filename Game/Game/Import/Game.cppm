export module gs;

import gse;

export import :main_test_scene;
export import :skybox_scene;
export import :second_test_scene;
export import :physics_stress_test_scene;
export import :physics_joint_test_scene;
export import :client;
export import :player;
export import :entity_builders;
export import :arena;
export import :tumbler;
export import :client_ui;
export import :crosshair_screen;
export import :crosshair_system;
export import :main_menu_screen;
export import :network_screen;
export import :pause_menu_system;
export import :settings_screen;
export import :world_loader;

export namespace gs {
	using networked_components = gse::network::engine_components;
}
