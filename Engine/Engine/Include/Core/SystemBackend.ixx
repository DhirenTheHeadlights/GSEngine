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

export namespace gse::systems {
	auto initialize() -> void;
	auto update() -> void;
	auto render() -> void;
	auto shutdown() -> void;
}

namespace gse::core {
	template <auto Initialize, auto Update, auto Render, auto Shutdown, auto... Dependencies>
	struct system;
}

namespace gse::system {
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
	template <auto Fn>
	concept callable = requires {
		requires std::is_invocable_v<decltype(Fn)> || Fn == nullptr;
	};

	template <auto Fn>
	concept valid_fn = requires {
		requires (std::is_invocable_v<decltype(Fn)> || Fn == nullptr);
	};

	template <
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

	template <typename T>
	constexpr auto system_id() -> std::type_index {
		return std::type_index(typeid(T));
	}

	template <typename System>
	auto get_dependencies() -> std::vector<std::type_index> {
		std::vector<std::type_index> result;
		std::apply([&]<typename... T0>(T0... dep) {
			(result.push_back(system_id<T0>()), ...);
			}, System::dependencies);
		return result;
	}

	template <typename... Systems>
	auto build_dependency_graph() -> std::unordered_map<std::type_index, std::vector<std::type_index>> {
		std::unordered_map<std::type_index, std::vector<std::type_index>> dependency_graph;
		((dependency_graph[system_id<Systems>()] = get_dependencies<Systems>()), ...);
		return dependency_graph;
	}

	template<typename... Systems>
	auto topologically_sorted_systems() -> std::vector<std::type_index> {
		auto graph = build_dependency_graph<Systems...>();
		std::unordered_set<std::type_index> visited, visiting;
		std::vector<std::type_index> sorted;

		auto dfs = [&](const std::type_index& node) {
			if (visited.contains(node)) return;

			perma_assert(!visiting.contains(node), "Cyclic dependency detected in system initialization.");

			visiting.insert(node);

			for (const auto& dep : graph[node]) {
				dfs(dep);
			}

			visiting.erase(node);
			visited.insert(node);

			sorted.push_back(node);
			};

		(dfs(system_id<Systems>()), ...);

		return sorted;
	}

	template <typename... Systems, typename Func>
	auto dispatch_system_by_type(const std::type_index& id, Func&& fn) -> void {
		((id == std::type_index(typeid(Systems)) ? (fn.template operator() < Systems > (), true) : false) || ...);
	}

	template <typename Tuple, std::size_t... Is>
	auto topologically_sorted_tuple_impl(std::index_sequence<Is...>) {
		return topologically_sorted_systems<std::tuple_element_t<Is, Tuple>...>();
	}

	template <typename Tuple>
	auto topologically_sorted_tuple() {
		constexpr std::size_t size = std::tuple_size_v<Tuple>;
		return topologically_sorted_tuple_impl<Tuple>(std::make_index_sequence<size>{});
	}

	template <typename Tuple, typename Func>
	auto dispatch_over_tuple(const std::type_index& id, Func&& fn) -> void {
		std::apply([&]<typename... T0>(T0... type) {
			dispatch_system_by_type<T0...>(id, std::forward<Func>(fn));
			}, Tuple{});
	}
}

using engine = std::tuple<
	gse::system::window,
	gse::system::input,
	gse::system::renderer2d,
	gse::system::renderer3d,
	gse::system::gui,
	gse::system::debug,
	gse::system::scene_loader,
	gse::system::clock
>;

auto gse::systems::initialize() -> void {
	for (const auto& id : core::topologically_sorted_tuple<engine>()) {
		core::dispatch_over_tuple<engine>(id, []<typename T>() {
			T::initialize();
		});
	}
}

auto gse::systems::update() -> void {
	for (const auto& id : core::topologically_sorted_tuple<engine>()) {
		core::dispatch_over_tuple<engine>(id, []<typename T>() {
			T::update();
		});
	}
}

auto gse::systems::render() -> void {
	for (const auto& id : core::topologically_sorted_tuple<engine>()) {
		core::dispatch_over_tuple<engine>(id, []<typename T>() {
			T::render();
		});
	}
}

auto gse::systems::shutdown() -> void {
	auto sorted = core::topologically_sorted_tuple<engine>();
	std::ranges::reverse(sorted); // Reverse the order for shutdown

	for (const auto& id : sorted) {
		core::dispatch_over_tuple<engine>(id, []<typename T>() {
			T::shutdown();
		});
	}
}