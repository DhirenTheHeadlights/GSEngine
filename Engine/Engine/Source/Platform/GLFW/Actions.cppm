export module gse.platform:actions;

import std;

import gse.utility;

import :keys;
import :input_state;

export namespace gse::actions {
	struct index {
		std::uint16_t value{};

		constexpr operator std::uint16_t() const {
			return value;
		}
	};

	constexpr index invalid{ std::numeric_limits<std::uint16_t>::max() };

	class description : public identifiable {
	public:
		explicit description(
			const std::string_view name,
			const index index = invalid
		) : identifiable(name), m_index(index) {}

		auto index() const -> index {
			return m_index;
		}
	private:
		actions::index m_index;
	};

	auto add(
		std::string_view tag,
		key default_key
	) -> index;

	auto load_rebinds(
	) -> void;

	auto finalize_bindings(
	) -> void;          

	auto get(
		const id& action_id
	) -> description*;

	auto index_of(
		const id& action_id
	) -> index;

	auto count(
	) -> std::size_t;

	auto all(
	) -> std::span<const description>;
}

namespace gse::actions {
	id_mapped_collection<description> descriptions;

	auto add(
		std::string_view tag
	) -> description&;
}

auto gse::actions::add(const std::string_view tag) -> description& {
	description desc(tag, static_cast<index>(descriptions.size()));
	return *descriptions.add(desc.id(), std::move(desc));
}

export namespace gse::actions {
	using word = std::uint64_t;

	class mask {
	public:
		auto ensure_for(
			std::size_t action_count
		) -> void;

		static auto wb(
			index a
		) -> std::pair<std::size_t, word>;

		auto set(
			index a
		) -> void;

		auto clear(
			index a
		) -> void;

		auto test(
			index a
		) const -> bool;

		auto reset(
		) -> void;
	private:
		std::vector<word> m_words;
	};

	using axis = unitless::vec2;
}

auto gse::actions::mask::ensure_for(const std::size_t action_count) -> void {
	m_words.resize((action_count + 63) / 64, 0);
}

auto gse::actions::mask::wb(const index a) -> std::pair<std::size_t, word> {
	return { a / 64, static_cast<word>(1) << (a % 64) };
}

auto gse::actions::mask::set(const index a) -> void {
	auto [word_index, bit] = wb(a);
	if (word_index >= m_words.size()) {
		return;
	}
	m_words[word_index] |= bit;
}

auto gse::actions::mask::clear(const index a) -> void {
	auto [word_index, bit] = wb(a);
	if (word_index >= m_words.size()) {
		return;
	}
	m_words[word_index] &= ~bit;
}

auto gse::actions::mask::test(const index a) const -> bool {
	auto [word_index, bit] = wb(a);
	if (word_index >= m_words.size()) {
		return false;
	}
	return (m_words[word_index] & bit) != 0;
}

auto gse::actions::mask::reset() -> void {
	std::ranges::fill(m_words, 0);
}

namespace gse::actions {
	class state {
	public:
		auto begin_frame(
		) -> void;

		auto ensure_capacity(
		) -> void;

		auto set_pressed(
			index a
		) -> void;

		auto set_released(
			index a
		) -> void;

		auto set_held(
			index a,
			bool on
		) -> void;

		auto held(
			index a
		) const -> bool;

		auto pressed(
			index a
		) const -> bool;

		auto released(
			index a
		) const -> bool;

		auto set_axis1(
			std::uint16_t id,
			float v
		) -> void;

		auto set_axis2(
			std::uint16_t id, 
			axis v
		) -> void;

		auto axis1(
			std::uint16_t id
		) const -> float;

		auto axis2_v(
			std::uint16_t id
		) const -> axis;

		auto held_mask(
		) const -> const mask&;

		auto pressed_mask(
		) const -> const mask&;

		auto released_mask(
		) const -> const mask&;
	private:
		mask m_held, m_pressed, m_released;
		std::unordered_map<std::uint16_t, float> m_axes1;
		std::unordered_map<std::uint16_t, axis> m_axes2;
	};

	state global_state;
}

auto gse::actions::state::begin_frame() -> void {
	m_pressed.reset();
	m_released.reset();
}

auto gse::actions::state::ensure_capacity() -> void {
	const auto n = count();
	m_held.ensure_for(n);
	m_pressed.ensure_for(n);
	m_released.ensure_for(n);
}

auto gse::actions::state::set_pressed(const index a) -> void {
	ensure_capacity();
	m_pressed.set(a);
	m_held.set(a);
}

auto gse::actions::state::set_released(const index a) -> void {
	ensure_capacity();
	m_released.set(a);
	m_held.clear(a);
}

auto gse::actions::state::set_held(const index a, const bool on) -> void {
	ensure_capacity();
	if (on) {
		m_held.set(a);
	} else {
		m_held.clear(a);
	}
}

auto gse::actions::state::held(const index a) const -> bool {
	return m_held.test(a);
}

auto gse::actions::state::pressed(const index a) const -> bool {
	return m_pressed.test(a);
}

auto gse::actions::state::released(const index a) const -> bool {
	return m_released.test(a);
}

auto gse::actions::state::set_axis1(const std::uint16_t id, const float v) -> void {
	m_axes1[id] = v;
}

auto gse::actions::state::set_axis2(const std::uint16_t id, const axis v) -> void {
	m_axes2[id] = v;
}

auto gse::actions::state::axis1(const std::uint16_t id) const -> float {
	if (const auto it = m_axes1.find(id); it != m_axes1.end()) {
		return it->second;
	}
	return 0.f;
}

auto gse::actions::state::axis2_v(const std::uint16_t id) const -> axis {
	if (const auto it = m_axes2.find(id); it != m_axes2.end()) {
		return it->second;
	}
	return {};
}

auto gse::actions::state::held_mask() const -> const mask& {
	return m_held;
}

auto gse::actions::state::pressed_mask() const -> const mask& {
	return m_pressed;
}

auto gse::actions::state::released_mask() const -> const mask& {
	return m_released;
}

export namespace gse::actions {
	auto held(
		index a
	) -> bool;

	auto pressed(
		index a
	) -> bool;

	auto released(
		index a
	) -> bool;

	auto axis1(
		std::uint16_t id
	) -> float;

	auto axis2_v(
		std::uint16_t id
	) -> axis;
}

auto gse::actions::held(const index a) -> bool {
	return global_state.held(a);
}

auto gse::actions::pressed(const index a) -> bool {
	return global_state.pressed(a);
}

auto gse::actions::released(const index a) -> bool {
	return global_state.released(a);
}

auto gse::actions::axis1(const std::uint16_t id) -> float {
	return global_state.axis1(id);
}

auto gse::actions::axis2_v(const std::uint16_t id) -> axis {
	return global_state.axis2_v(id);
}

namespace gse::actions {
	struct bindings {
		std::vector<std::pair<key, index>> key_to_action;
		std::vector<std::pair<mouse_button, index>> mouse_to_action;

		struct key_axis1 {
			key neg;
			key pos;
			std::uint16_t axis;
			float scale = 1.f;
		};

		std::vector<key_axis1> axes1_from_keys;

		struct mouse_axis2 {
			std::uint16_t axis;
			float px_to_x = 0.1f,
			px_to_y = 0.1f;
		};

		std::vector<mouse_axis2> axes2_from_mouse;
	};

	auto map_to_actions(
		const input::state& in
	) -> void;

	struct pending_key_binding {
		std::string name;
		key def;
		index index;
	};

	std::vector<pending_key_binding> pending_key_bindings;
	std::unordered_map<std::string, key> rebinds;
	bindings resolved;
}

auto gse::actions::map_to_actions(const input::state& in) -> void {
	global_state.begin_frame();
	global_state.ensure_capacity();

	for (auto& [k, a] : resolved.key_to_action) {
		if (in.key_pressed(k)) {
			global_state.set_pressed(a);
		}
		if (in.key_released(k)) {
			global_state.set_released(a);
		}
		global_state.set_held(a, in.key_held(k));
	}

	for (auto& [mb, a] : resolved.mouse_to_action) {
		if (in.mouse_button_pressed(mb)) {
			global_state.set_pressed(a);
		}
		if (in.mouse_button_released(mb)) {
			global_state.set_released(a);
		}
		global_state.set_held(a, in.mouse_button_held(mb));
	}

	for (const auto& [neg, pos, axis, scale] : resolved.axes1_from_keys) {
		const int v = (in.key_held(pos) ? 1 : 0) - (in.key_held(neg) ? 1 : 0);
		global_state.set_axis1(axis, static_cast<float>(v) * scale);
	}

	const auto md = in.mouse_delta();
	for (const auto& [axis, px_to_x, px_to_y] : resolved.axes2_from_mouse) {
		global_state.set_axis2(axis, { md.x() * px_to_x, md.y() * px_to_y });
	}
}

auto gse::actions::add(const std::string_view tag, key default_key) -> index {
	const auto index = add(tag).index();

	pending_key_bindings.emplace_back(
	  std::string(tag),
	  default_key,
	  index	
	);

	return index;
}

auto gse::actions::load_rebinds() -> void {
	// TODO: Load from file or user settings
}

auto gse::actions::finalize_bindings() -> void {
	resolved = {};
	for (const auto& [name, def, index] : pending_key_bindings) {
		const key k = (rebinds.contains(name) ? rebinds[name] : def);
		resolved.key_to_action.emplace_back(k, index);
	}
}

auto gse::actions::get(const id& action_id) -> description* {
	return descriptions.try_get(action_id);
}

auto gse::actions::index_of(const id& action_id) -> index {
	if (const auto* desc = get(action_id)) {
		return desc->index();
	}
	return invalid;
}

auto gse::actions::count() -> std::size_t {
	return descriptions.size();
}

auto gse::actions::all() -> std::span<const description> {
	return descriptions.items();
}