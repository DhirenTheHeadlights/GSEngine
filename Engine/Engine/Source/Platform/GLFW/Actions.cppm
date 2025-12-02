export module gse.platform:actions;

import std;

import gse.utility;
import gse.assert;

import :keys;
import :input_state;

namespace gse::actions {
	template <std::size_t N>
	struct fixed_string {
		char v[N];

		constexpr fixed_string(const char(&s)[N]) {
			for (std::size_t i = 0; i < N; ++i) {
				v[i] = s[i];
			}
		}

		constexpr auto view() const -> std::string_view {
			return { v, N - 1 };
		}
	};

	template <std::size_t N>
	fixed_string(const char(&)[N]) -> fixed_string<N>;
}

export namespace gse::actions {
	class description : public identifiable {
	public:
		explicit description(
			const std::string_view name,
			const std::uint16_t bit_index
		) : identifiable(name), m_bit_index(bit_index) {
		}

		auto bit_index(
		) const -> std::uint16_t {
			return m_bit_index;
		}
	private:
		std::uint16_t m_bit_index{};
	};

	template <fixed_string Tag>
	auto add(
		key default_key
	) -> id;

	auto load_rebinds(
	) -> void;

	auto finalize_bindings(
	) -> void;

	auto get(
		id action_id
	) -> description*;

	auto count(
	) -> std::size_t;

	auto all(
	) -> std::span<const description>;
}

namespace gse::actions {
	id_mapped_collection<description> descriptions;

	auto add_description(
		std::string_view tag
	) -> description&;
}

auto gse::actions::add_description(const std::string_view tag) -> description& {
	const auto bit_index = static_cast<std::uint16_t>(descriptions.size());
	description desc(tag, bit_index);
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
			std::uint16_t bit_index
		) -> std::pair<std::size_t, word>;

		auto set(
			std::uint16_t bit_index
		) -> void;

		auto clear(
			std::uint16_t bit_index
		) -> void;

		auto test(
			std::uint16_t bit_index
		) const -> bool;

		auto reset(
		) -> void;

		auto word_count(
		) const -> std::size_t;

		auto words(
		) const -> std::span<const word>;

		auto assign(
			std::span<const word> w
		) -> void;
	private:
		std::vector<word> m_words;
	};

	using axis = unitless::vec2;
}

auto gse::actions::mask::ensure_for(const std::size_t action_count) -> void {
	m_words.resize((action_count + 63) / 64, 0);
}

auto gse::actions::mask::wb(const std::uint16_t bit_index) -> std::pair<std::size_t, word> {
	return { bit_index / 64, static_cast<word>(1) << (bit_index % 64) };
}

auto gse::actions::mask::set(const std::uint16_t bit_index) -> void {
	auto [word_index, bit] = wb(bit_index);
	if (word_index >= m_words.size()) {
		return;
	}
	m_words[word_index] |= bit;
}

auto gse::actions::mask::clear(const std::uint16_t bit_index) -> void {
	auto [word_index, bit] = wb(bit_index);
	if (word_index >= m_words.size()) {
		return;
	}
	m_words[word_index] &= ~bit;
}

auto gse::actions::mask::test(const std::uint16_t bit_index) const -> bool {
	auto [word_index, bit] = wb(bit_index);
	if (word_index >= m_words.size()) {
		return false;
	}
	return (m_words[word_index] & bit) != 0;
}

auto gse::actions::mask::reset() -> void {
	std::ranges::fill(m_words, 0);
}

auto gse::actions::mask::word_count() const -> std::size_t {
	return m_words.size();
}

auto gse::actions::mask::words() const -> std::span<const word> {
	return m_words;
}

auto gse::actions::mask::assign(const std::span<const word> w) -> void {
	m_words.assign(w.begin(), w.end());
}

export namespace gse::actions {
	class state {
	public:
		auto begin_frame(
		) -> void;

		auto ensure_capacity(
		) -> void;

		auto set_pressed(
			std::uint16_t bit_index
		) -> void;

		auto set_released(
			std::uint16_t bit_index
		) -> void;

		auto set_held(
			std::uint16_t bit_index,
			bool on
		) -> void;

		auto held(
			std::uint16_t bit_index
		) const -> bool;

		auto pressed(
			std::uint16_t bit_index
		) const -> bool;

		auto released(
			std::uint16_t bit_index
		) const -> bool;

		auto held(
			id action_id
		) const -> bool;

		auto pressed(
			id action_id
		) const -> bool;

		auto released(
			id action_id
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

		auto axis1(
			id axis_id
		) const -> float;

		auto axis2_v(
			id axis_id
		) const -> axis;

		auto held_mask(
		) const -> const mask&;

		auto pressed_mask(
		) const -> const mask&;

		auto released_mask(
		) const -> const mask&;

		auto load_transients(
			std::span<const word> pressed,
			std::span<const word> released
		) -> void;
	private:
		mask m_held;
		mask m_pressed;
		mask m_released;
		std::unordered_map<std::uint16_t, float> m_axes1;
		std::unordered_map<std::uint16_t, axis> m_axes2;
	};
}

namespace gse::actions {
	double_buffer<state> states_db;
	std::vector<std::uint16_t> axis1_ids_cache;
	std::vector<std::uint16_t> axis2_ids_cache;
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

auto gse::actions::state::set_pressed(const std::uint16_t bit_index) -> void {
	ensure_capacity();
	m_pressed.set(bit_index);
	m_held.set(bit_index);
}

auto gse::actions::state::set_released(const std::uint16_t bit_index) -> void {
	ensure_capacity();
	m_released.set(bit_index);
	m_held.clear(bit_index);
}

auto gse::actions::state::set_held(const std::uint16_t bit_index, const bool on) -> void {
	ensure_capacity();
	if (on) {
		m_held.set(bit_index);
	} else {
		m_held.clear(bit_index);
	}
}

auto gse::actions::state::held(const std::uint16_t bit_index) const -> bool {
	return m_held.test(bit_index);
}

auto gse::actions::state::pressed(const std::uint16_t bit_index) const -> bool {
	return m_pressed.test(bit_index);
}

auto gse::actions::state::released(const std::uint16_t bit_index) const -> bool {
	return m_released.test(bit_index);
}

auto gse::actions::state::held(const id action_id) const -> bool {
	if (const auto* desc = get(action_id)) {
		return held(desc->bit_index());
	}
	return false;
}

auto gse::actions::state::pressed(const id action_id) const -> bool {
	if (const auto* desc = get(action_id)) {
		return pressed(desc->bit_index());
	}
	return false;
}

auto gse::actions::state::released(const id action_id) const -> bool {
	if (const auto* desc = get(action_id)) {
		return released(desc->bit_index());
	}
	return false;
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

auto gse::actions::state::axis1(const id axis_id) const -> float {
	return axis1(static_cast<std::uint16_t>(axis_id.number()));
}

auto gse::actions::state::axis2_v(const id axis_id) const -> axis {
	return axis2_v(static_cast<std::uint16_t>(axis_id.number()));
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

auto gse::actions::state::load_transients(const std::span<const word> pressed, const std::span<const word> released) -> void {
	ensure_capacity();

	m_pressed.assign(pressed);
	m_released.assign(released);

	const std::size_t wc = std::max(pressed.size(), released.size());

	m_held.ensure_for(wc * 64);

	std::vector held(m_held.words().begin(), m_held.words().end());
	held.resize(wc, 0);

	const auto pw = m_pressed.words();
	const auto rw = m_released.words();

	for (std::size_t i = 0; i < wc; ++i) {
		const word p = (i < pw.size() ? pw[i] : 0);
		const word r = (i < rw.size() ? rw[i] : 0);
		held[i] = (held[i] | p) & ~r;
	}
	m_held.assign(held);
}

export namespace gse::actions {
	struct button_channel {
		id action_id{};
		bool held = false;
		bool pressed = false;
		bool released = false;
	};

	struct axis1_channel {
		std::uint16_t axis_id{};
		float value = 0.f;
	};

	struct axis2_channel {
		id axis_id{};
		axis value{};
	};

	struct channel_binding {
		id owner;
		std::function<void(const state&)> sampler;
	};

	auto register_channel(
		id owner_id,
		button_channel& channel
	) -> void;

	auto register_channel(
		id owner_id,
		axis1_channel& channel
	) -> void;

	auto register_channel(
		id owner_id,
		axis2_channel& channel
	) -> void;

	auto sample_for_entity(
		const state& s,
		id owner_id
	) -> void;

	template <fixed_string Tag>
	auto bind_button_channel(
		id owner_id,
		key default_key,
		button_channel& channel
	) -> void;

	auto bind_axis2_channel(
		id owner_id,
		const struct pending_axis2_info& info,
		axis2_channel& channel,
		id axis_id
	) -> void;
}

namespace gse::actions {
	std::vector<channel_binding> channel_bindings;
}

auto gse::actions::register_channel(const id owner_id, button_channel& channel) -> void {
	channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&channel](const state& s) {
			channel.held = s.held(channel.action_id);
			channel.pressed = s.pressed(channel.action_id);
			channel.released = s.released(channel.action_id);
		}
	});
}

auto gse::actions::register_channel(const id owner_id, axis1_channel& channel) -> void {
	channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&channel](const state& s) {
			channel.value = s.axis1(channel.axis_id);
		}
	});
}

auto gse::actions::register_channel(const id owner_id, axis2_channel& channel) -> void {
	channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&channel](const state& s) {
			channel.value = s.axis2_v(channel.axis_id);
		}
	});
}

auto gse::actions::sample_for_entity(const state& s, const id owner_id) -> void {
	for (auto& b : channel_bindings) {
		if (b.owner == owner_id) {
			b.sampler(s);
		}
	}
}

template <gse::actions::fixed_string Tag>
auto gse::actions::bind_button_channel(const id owner_id, const key default_key, button_channel& channel) -> void {
	channel.action_id = add<Tag>(default_key);
	register_channel(owner_id, channel);
}

namespace gse::actions {
	struct bindings {
		std::vector<std::pair<key, std::uint16_t>> key_to_action;
		std::vector<std::pair<mouse_button, std::uint16_t>> mouse_to_action;

		struct key_axis1 {
			key neg;
			key pos;
			std::uint16_t axis;
			float scale = 1.f;
		};

		std::vector<key_axis1> axes1_from_keys;

		struct mouse_axis2 {
			std::uint16_t axis;
			float px_to_x = 0.1f;
			float px_to_y = 0.1f;
		};

		std::vector<mouse_axis2> axes2_from_mouse;
	};

	auto map_to_actions(
		const input::state& in
	) -> void;

	struct pending_key_binding {
		std::string name;
		key def;
		id action_id;
	};

	std::vector<pending_key_binding> pending_key_bindings;
	std::unordered_map<std::string, key> rebinds;
	bindings resolved;

	struct pending_axis2 {
		id left;
		id right;
		id back;
		id fwd;
		float scale = 1.f;
		id id;
	};

	export struct pending_axis2_info {
		id left;
		id right;
		id back;
		id fwd;
		float scale = 1.f;
	};

	struct resolved_axis2_keys {
		id id;
		key left;
		key right;
		key back;
		key fwd;
		float scale = 1.f;
	};

	export auto add_axis2_actions(
		const pending_axis2& pa
	) -> id;

	std::vector<pending_axis2> pending_axis2_key_bindings;
	id_mapped_collection<resolved_axis2_keys> axis2_by_id;
}

auto gse::actions::bind_axis2_channel(
	const id owner_id,
	const pending_axis2_info& info,
	axis2_channel& channel,
	const id axis_id
) -> void {
	const pending_axis2 pa{
		.left = info.left,
		.right = info.right,
		.back = info.back,
		.fwd = info.fwd,
		.scale = info.scale,
		.id = axis_id
	};

	channel.axis_id = add_axis2_actions(pa);
	register_channel(owner_id, channel);
}

export namespace gse::actions {
	auto axis1_ids(
	) -> std::span<const std::uint16_t>;

	auto axis2_ids(
	) -> std::span<const std::uint16_t>;

	auto current_state(
	) -> const state&;

	auto pressed_mask(
	) -> const mask&;

	auto released_mask(
	) -> const mask&;

	auto axis1(
		std::uint16_t id
	) -> float;

	auto axis2(
		std::uint16_t id
	) -> axis;
}

auto gse::actions::axis1_ids() -> std::span<const std::uint16_t> {
	return axis1_ids_cache;
}

auto gse::actions::axis2_ids() -> std::span<const std::uint16_t> {
	return axis2_ids_cache;
}

auto gse::actions::current_state() -> const state& {
	return states_db.read();
}

auto gse::actions::pressed_mask() -> const mask& {
	return states_db.read().pressed_mask();
}

auto gse::actions::released_mask() -> const mask& {
	return states_db.read().released_mask();
}

auto gse::actions::axis1(const std::uint16_t id) -> float {
	return states_db.read().axis1(id);
}

auto gse::actions::axis2(const std::uint16_t id) -> axis {
	return states_db.read().axis2_v(id);
}

auto gse::actions::map_to_actions(const input::state& in) -> void {
	auto& s = states_db.write();
	s.begin_frame();
	s.ensure_capacity();

	for (auto& [k, bit_index] : resolved.key_to_action) {
		if (in.key_pressed(k)) {
			s.set_pressed(bit_index);
		}
		if (in.key_released(k)) {
			s.set_released(bit_index);
		}
		s.set_held(bit_index, in.key_held(k));
	}

	for (auto& [mb, bit_index] : resolved.mouse_to_action) {
		if (in.mouse_button_pressed(mb)) {
			s.set_pressed(bit_index);
		}
		if (in.mouse_button_released(mb)) {
			s.set_released(bit_index);
		}
		s.set_held(bit_index, in.mouse_button_held(mb));
	}

	for (const auto& [neg, pos, axis, scale] : resolved.axes1_from_keys) {
		const int v = (in.key_held(pos) ? 1 : 0) - (in.key_held(neg) ? 1 : 0);
		s.set_axis1(axis, static_cast<float>(v) * scale);
	}

	for (const auto& [id, left, right, back, fwd, scale] : axis2_by_id.items()) {
		const int x = (in.key_held(right) ? 1 : 0) - (in.key_held(left) ? 1 : 0);
		const int y = (in.key_held(back) ? 1 : 0) - (in.key_held(fwd) ? 1 : 0);
		s.set_axis2(static_cast<std::uint16_t>(id.number()), { static_cast<float>(x) * scale, static_cast<float>(y) * scale });
	}

	states_db.flip();
}

template <gse::actions::fixed_string Tag>
auto gse::actions::add(key default_key) -> id {
	auto& desc = add_description(Tag.view());

	pending_key_bindings.emplace_back(
		std::string(Tag.view()),
		default_key,
		desc.id()
	);

	return desc.id();
}

auto gse::actions::load_rebinds() -> void {
}

auto gse::actions::finalize_bindings() -> void {
	resolved = {};
	for (const auto& [name, def, action_id] : pending_key_bindings) {
		const key k = (rebinds.contains(name) ? rebinds[name] : def);
		const auto* desc = descriptions.try_get(action_id);
		if (!desc) {
			continue;
		}
		const auto bit_index = desc->bit_index();
		resolved.key_to_action.emplace_back(k, bit_index);
	}

	axis2_by_id.clear();

	auto key_for_action = [&](const id action_id) -> key {
		const auto* desc = descriptions.try_get(action_id);
		if (!desc) {
			return key{};
		}
		const auto bit_index = desc->bit_index();
		for (const auto& [k, idx] : resolved.key_to_action) {
			if (idx == bit_index) {
				return k;
			}
		}
		return key{};
	};

	for (const auto& [left, right, back, fwd, scale, id] : pending_axis2_key_bindings) {
		resolved_axis2_keys r{
			.id = id,
			.left = key_for_action(left),
			.right = key_for_action(right),
			.back = key_for_action(back),
			.fwd = key_for_action(fwd),
			.scale = scale
		};
		axis2_by_id.add(r.id, std::move(r));
	}

	axis1_ids_cache.clear();
	axis1_ids_cache.reserve(resolved.axes1_from_keys.size());

	for (const auto& k : resolved.axes1_from_keys) {
		axis1_ids_cache.push_back(k.axis);
	}

	std::ranges::sort(axis1_ids_cache);
	axis1_ids_cache.erase(std::ranges::unique(axis1_ids_cache).begin(), axis1_ids_cache.end());

	axis2_ids_cache.clear();
	axis2_ids_cache.reserve(axis2_by_id.size());

	for (const auto& [id, left, right, back, fwd, scale] : axis2_by_id.items()) {
		axis2_ids_cache.push_back(static_cast<std::uint16_t>(id.number()));
	}

	std::ranges::sort(axis2_ids_cache);
	axis2_ids_cache.erase(std::ranges::unique(axis2_ids_cache).begin(), axis2_ids_cache.end());
}

auto gse::actions::get(const id action_id) -> description* {
	return descriptions.try_get(action_id);
}

auto gse::actions::count() -> std::size_t {
	return descriptions.size();
}

auto gse::actions::all() -> std::span<const description> {
	return descriptions.items();
}

auto gse::actions::add_axis2_actions(const pending_axis2& pa) -> id {
	pending_axis2_key_bindings.emplace_back(pa);
	return pa.id;
}
