export module gse.runtime:gui_api;

import std;

import gse.graphics;

import :core_api;

export namespace gse::gui {
	auto start(
		const std::string& name,
		const std::function<void()>& contents
	) -> void {
		system_of<system>().start(name, contents);
	}

	auto text(
		const std::string& text
	) -> void {
		system_of<system>().text(text);
	}

	auto button(
		const std::string& text
	) -> bool {
		return system_of<system>().button(text);
	}

	auto input(
		const std::string& name,
		std::string& buffer
	) -> void {
		system_of<system>().input(name, buffer);
	}

	template <is_arithmetic T>
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void {
		system_of<system>().slider(name, value, min, max);
	}

	template <is_quantity T, auto Unit = typename T::default_unit{} >
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void {
		system_of<system>().slider<T, Unit>(name, value, min, max);
	}

	template <typename T, int N>
	auto slider(
		const std::string& name,
		unitless::vec_t<T, N>& vec,
		unitless::vec_t<T, N> min,
		unitless::vec_t<T, N> max
	) -> void {
		system_of<system>().slider(name, vec, min, max);
	}

	template <typename T, int N, auto Unit = typename T::default_unit{} >
	auto slider(
		const std::string& name,
		vec_t<T, N>& vec,
		vec_t<T, N> min,
		vec_t<T, N> max
	) -> void {
		system_of<system>().slider<T, N, Unit>(name, vec, min, max);
	}

	template <is_arithmetic T>
	auto value(
		const std::string& name,
		T val
	) -> void {
		system_of<system>().value(name, val);
	}

	template <is_quantity T, auto Unit = typename T::default_unit{}>
	auto value(
		const std::string& name,
		T val
	) -> void {
		system_of<system>().value<T, Unit>(name, val);
	}

	template <typename T, int N>
	auto vec(
		const std::string& name,
		unitless::vec_t<T, N> val
	) -> void {
		system_of<system>().vec(name, val);
	}

	template <typename T, int N, auto Unit = typename T::default_unit{}>
	auto vec(
		const std::string& name,
		vec_t<T, N> val
	) -> void {
		system_of<system>().vec<T, N, Unit>(name, val);
	}

	template <typename T>
	auto tree(
		std::span<const T> roots,
		const draw::tree_ops<T>& fns,
		draw::tree_options opt = {},
		draw::tree_selection* sel = nullptr
	) -> bool {
		return system_of<system>().tree(roots, fns, opt, sel);
	}

	auto selectable(
		const std::string& text,
		const bool selected = false
	) -> bool {
		return system_of<system>().selectable(text, selected);
	}

	auto profiler(
	) -> void {
		system_of<system>().profiler();
	}
}