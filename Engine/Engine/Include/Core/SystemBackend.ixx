export module gse.core.system_backend;

import std;

import gse.platform.perma_assert;
import gse.core.main_clock;
import gse.core.scene_loader;
import gse.graphics.debug;
import gse.graphics.gui;
import gse.graphics.renderer2d;
import gse.graphics.renderer3d;
import gse.platform.glfw.input;
import gse.platform.glfw.window;

namespace gse::core {
	template<auto Initialize, auto Update, auto Render, auto Shutdown, auto... Dependencies>
	struct system;
}

export namespace gse::system {
	using clock			= core::system<nullptr, &main_clock::update, nullptr, nullptr>;
	using scene_loader	= core::system<nullptr, &scene_loader::update, &scene_loader::render, &scene_loader::exit>;
	using renderer2d	= core::system<&renderer2d::initialize, nullptr, nullptr, &renderer2d::shutdown>;
	using renderer3d	= core::system<&renderer3d::initialize, nullptr, nullptr, nullptr>;
	using gui			= core::system<&gui::initialize, &gui::update, &gui::render, &gui::shutdown>;
	using input			= core::system<&input::initialize, &input::update, nullptr, nullptr>;
	using debug			= core::system<&debug::initialize, &debug::update, &debug::render, nullptr>;
	using window		= core::system<&window::initialize, &window::update, nullptr, &window::shutdown>;
}

namespace gse::core {
	template<auto Fn>
	concept callable = requires {
		requires std::is_invocable_v<decltype(Fn)> || Fn == nullptr;
	};

	template<auto Fn>
	concept valid_fn = requires {
		requires (std::is_invocable_v<decltype(Fn)> || Fn == nullptr);
	};

	template<
		auto Initialize,
		auto Update,
		auto Render,
		auto Shutdown,
		typename... Dependencies>
	requires
		valid_fn<Initialize> &&
		valid_fn<Update>	 &&
		valid_fn<Render>	 &&
		valid_fn<Shutdown>
	struct system {
		constexpr static auto initialize = Initialize;
		constexpr static auto update = Update;
		constexpr static auto render = Render;
		constexpr static auto shutdown = Shutdown;
		constexpr static auto dependencies = std::tuple<Dependencies...>{};
	};
}

namespace call {
	template<typename System> auto initialize() -> void {
		if constexpr (std::is_invocable_v<decltype(System::initialize)>) {
			System::initialize();
		}
	}
	template<typename System> auto update() -> void {
		if constexpr (std::is_invocable_v<decltype(System::update)>) {
			System::update();
		}
	}
	template<typename System> auto render() -> void {
		if constexpr (std::is_invocable_v<decltype(System::render)>) {
			System::render();
		}
	}
	template<typename System> auto shutdown() -> void {
		if constexpr (std::is_invocable_v<decltype(System::shutdown)>) {
			System::shutdown();
		}
	}
}