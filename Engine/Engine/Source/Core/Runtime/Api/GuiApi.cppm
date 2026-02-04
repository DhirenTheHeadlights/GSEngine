export module gse.runtime:gui_api;

import std;

import gse.graphics;
import gse.platform;

import :core_api;

export namespace gse::gui {
	auto start(
		const std::string& name,
		const std::function<void()>& contents
	) -> void {
		auto& gs = state_of<system_state>();
		const auto& is = state_of<input::system_state>().current_state();
		system::start(gs, is, name, contents);
	}

	auto text(
		const std::string& text
	) -> void {
		system::text(state_of<system_state>(), text);
	}

	auto button(
		const std::string& text
	) -> bool {
		return system::button(state_of<system_state>(), text);
	}

	auto input(
		const std::string& name,
		std::string& buffer
	) -> void {
		system::input(state_of<system_state>(), name, buffer);
	}

	template <is_arithmetic T>
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void {
		system::slider(state_of<system_state>(), name, value, min, max);
	}

	template <is_quantity T, auto Unit = typename T::default_unit{} >
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void {
		system::slider<T, Unit>(state_of<system_state>(), name, value, min, max);
	}

	template <typename T, int N>
	auto slider(
		const std::string& name,
		unitless::vec_t<T, N>& vec,
		unitless::vec_t<T, N> min,
		unitless::vec_t<T, N> max
	) -> void {
		system::slider(state_of<system_state>(), name, vec, min, max);
	}

	template <typename T, int N, auto Unit = typename T::default_unit{} >
	auto slider(
		const std::string& name,
		vec_t<T, N>& vec,
		vec_t<T, N> min,
		vec_t<T, N> max
	) -> void {
		system::slider<T, N, Unit>(state_of<system_state>(), name, vec, min, max);
	}

	template <is_arithmetic T>
	auto value(
		const std::string& name,
		T val
	) -> void {
		system::value(state_of<system_state>(), name, val);
	}

	template <is_quantity T, auto Unit = typename T::default_unit{}>
	auto value(
		const std::string& name,
		T val
	) -> void {
		system::value<T, Unit>(state_of<system_state>(), name, val);
	}

	template <typename T, int N>
	auto vec(
		const std::string& name,
		unitless::vec_t<T, N> val
	) -> void {
		system::vec(state_of<system_state>(), name, val);
	}

	template <typename T, int N, auto Unit = typename T::default_unit{}>
	auto vec(
		const std::string& name,
		vec_t<T, N> val
	) -> void {
		system::vec<T, N, Unit>(state_of<system_state>(), name, val);
	}

	template <typename T>
	auto tree(
		std::span<const T> roots,
		const draw::tree_ops<T>& fns,
		draw::tree_options opt = {},
		draw::tree_selection* sel = nullptr
	) -> bool {
		return system::tree(state_of<system_state>(), roots, fns, opt, sel);
	}

	auto selectable(
		const std::string& text,
		const bool selected = false
	) -> bool {
		return system::selectable(state_of<system_state>(), text, selected);
	}

	auto profiler(
	) -> void {
		system::profiler(state_of<system_state>());
	}
}
