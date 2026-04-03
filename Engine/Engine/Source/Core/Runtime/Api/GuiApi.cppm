export module gse.runtime:gui_api;

import std;

import gse.graphics;
import gse.platform;

import :core_api;

export namespace gse::gui {
	auto start(
		const std::string& name,
		const std::function<void()>& contents
	) -> void;

	auto text(
		const std::string& text
	) -> void;

	auto button(
		const std::string& text
	) -> bool;

	auto input(
		const std::string& name,
		std::string& buffer
	) -> void;

	template <is_arithmetic T>
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void;

	template <is_quantity T, auto Unit = typename T::default_unit{}>
	auto slider(
		const std::string& name,
		T& value,
		T min,
		T max
	) -> void;

	template <typename T, int N> requires std::is_arithmetic_v<T>
	auto slider(
		const std::string& name,
		gse::vec<T, N>& v,
		gse::vec<T, N> min,
		gse::vec<T, N> max
	) -> void;

	template <typename T, int N, auto Unit = typename T::default_unit{}> requires internal::is_arithmetic_wrapper<T>
	auto slider(
		const std::string& name,
		gse::vec<T, N>& v,
		gse::vec<T, N> min,
		gse::vec<T, N> max
	) -> void;

	template <is_arithmetic T>
	auto value(
		const std::string& name,
		T val
	) -> void;

	template <is_quantity T, auto Unit = typename T::default_unit{}>
	auto value(
		const std::string& name,
		T val
	) -> void;

	template <auto Unit, is_quantity T>
	auto value(
		const std::string& name,
		unit_display<Unit, T> ud
	) -> void;

	template <typename T, int N> requires std::is_arithmetic_v<T>
	auto vec(
		const std::string& name,
		gse::vec<T, N> val
	) -> void;

	template <typename T, int N, auto Unit = typename T::default_unit{}> requires internal::is_arithmetic_wrapper<T>
	auto vec(
		const std::string& name,
		gse::vec<T, N> val
	) -> void;

	template <auto Unit, typename T, int N>
	auto vec(
		const std::string& name,
		unit_display<Unit, gse::vec<T, N>> ud
	) -> void;

	template <typename T>
	auto tree(
		std::span<const T> roots,
		const draw::tree_ops<T>& fns,
		draw::tree_options opt = {},
		draw::tree_selection* sel = nullptr
	) -> bool;

	auto selectable(
		const std::string& text,
		const bool selected = false
	) -> bool;

	auto profiler(
	) -> void;
}

auto gse::gui::start(const std::string& name, const std::function<void()>& contents) -> void {
	auto& gs = state_of<system_state>();
	const auto& is = state_of<input::system_state>().current_state();
	system::start(gs, is, name, contents);
}

auto gse::gui::text(const std::string& text) -> void {
	system::text(state_of<system_state>(), text);
}

auto gse::gui::button(const std::string& text) -> bool {
	return system::button(state_of<system_state>(), text);
}

auto gse::gui::input(const std::string& name, std::string& buffer) -> void {
	system::input(state_of<system_state>(), name, buffer);
}

template <gse::is_arithmetic T>
auto gse::gui::slider(const std::string& name, T& value, T min, T max) -> void {
	system::slider(state_of<system_state>(), name, value, min, max);
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::slider(const std::string& name, T& value, T min, T max) -> void {
	system::slider<T, Unit>(state_of<system_state>(), name, value, min, max);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
auto gse::gui::slider(const std::string& name, gse::vec<T, N>& v, gse::vec<T, N> min, gse::vec<T, N> max) -> void {
	system::slider(state_of<system_state>(), name, v, min, max);
}

template <typename T, int N, auto Unit> requires gse::internal::is_arithmetic_wrapper<T>
auto gse::gui::slider(const std::string& name, gse::vec<T, N>& v, gse::vec<T, N> min, gse::vec<T, N> max) -> void {
	system::slider<T, N, Unit>(state_of<system_state>(), name, v, min, max);
}

template <gse::is_arithmetic T>
auto gse::gui::value(const std::string& name, T val) -> void {
	system::value(state_of<system_state>(), name, val);
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::value(const std::string& name, T val) -> void {
	system::value<T, Unit>(state_of<system_state>(), name, val);
}

template <auto Unit, gse::is_quantity T>
auto gse::gui::value(const std::string& name, unit_display<Unit, T> ud) -> void {
	system::value(state_of<system_state>(), name, ud);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
auto gse::gui::vec(const std::string& name, gse::vec<T, N> val) -> void {
	system::vec(state_of<system_state>(), name, val);
}

template <typename T, int N, auto Unit> requires gse::internal::is_arithmetic_wrapper<T>
auto gse::gui::vec(const std::string& name, gse::vec<T, N> val) -> void {
	system::vec<T, N, Unit>(state_of<system_state>(), name, val);
}

template <auto Unit, typename T, int N>
auto gse::gui::vec(const std::string& name, unit_display<Unit, gse::vec<T, N>> ud) -> void {
	system::vec(state_of<system_state>(), name, ud);
}

template <typename T>
auto gse::gui::tree(std::span<const T> roots, const draw::tree_ops<T>& fns, draw::tree_options opt, draw::tree_selection* sel) -> bool {
	return system::tree(state_of<system_state>(), roots, fns, opt, sel);
}

auto gse::gui::selectable(const std::string& text, const bool selected) -> bool {
	return system::selectable(state_of<system_state>(), text, selected);
}

auto gse::gui::profiler() -> void {
	system::profiler(state_of<system_state>());
}
