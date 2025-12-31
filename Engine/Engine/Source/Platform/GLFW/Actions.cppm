export module gse.platform:actions;

import std;

import gse.utility;
import gse.assert;

import :input;
import :keys;
import :input_state;

namespace gse::actions {
	template <std::size_t N>
	struct fixed_string {
		char v[N];

		explicit constexpr fixed_string(const char(&s)[N]) {
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

	using word = std::uint64_t;
	using axis = unitless::vec2;

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

	class state {
	public:
		auto begin_frame(
		) -> void;

		auto ensure_capacity(
			std::size_t count
		) -> void;

		auto set_pressed(
			std::uint16_t bit_index,
			std::size_t count
		) -> void;

		auto set_released(
			std::uint16_t bit_index,
			std::size_t count
		) -> void;

		auto set_held(
			std::uint16_t bit_index,
			bool on,
			std::size_t count
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

	struct pending_axis2_info {
		id left;
		id right;
		id back;
		id fwd;
		float scale = 1.f;
	};

	class system final : public basic_system {
	public:
		system() = default;
		~system() override = default;

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;

		template <fixed_string Tag>
		auto add(
			key default_key
		) -> id;

		auto finalize_bindings(
		) -> void;

		auto current_state(
		) const -> const state&;

		auto axis1_ids(
		) const -> std::span<const std::uint16_t>;

		auto axis2_ids(
		) const -> std::span<const std::uint16_t>;

		auto bind_axis2(
			const pending_axis2_info& info
		) -> id;

		auto register_channel_binding(
			id owner,
			std::function<void(const state&)> sampler
		) -> void;

		auto sample_all_channels(
			const state& s
		) const -> void;

		auto sample_for_entity(
			const state& s,
			id owner_id
		) const -> void;

		auto description(
			id action_id
		) -> description*;

	private:
		auto add_description(
			std::string_view tag
		) -> actions::description&;

		double_buffer<state> m_states;

		struct channel_binding {
			id owner;
			std::function<void(const state&)> sampler;
		};
		std::vector<channel_binding> m_channel_bindings;

		id_mapped_collection<actions::description> m_descriptions;

		struct pending_key_binding {
			std::string name;
			key def;
			id action_id;
		};
		std::vector<pending_key_binding> m_pending_key_bindings;
		std::unordered_map<std::string, key> m_rebinds;

		struct pending_axis2_req {
			id left;
			id right;
			id back;
			id fwd;
			float scale = 1.f;
			id id;
		};
		std::vector<pending_axis2_req> m_pending_axis2_reqs;

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
		} m_resolved;

		std::vector<std::uint16_t> m_axis1_ids_cache;
		std::vector<std::uint16_t> m_axis2_ids_cache;

		struct resolved_axis2_keys {
			id id;
			key left;
			key right;
			key back;
			key fwd;
			float scale = 1.f;
		};
		id_mapped_collection<resolved_axis2_keys> m_axis2_by_id;
	};
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

auto gse::actions::state::begin_frame() -> void {
	m_pressed.reset();
	m_released.reset();
}

auto gse::actions::state::ensure_capacity(const std::size_t count) -> void {
	m_held.ensure_for(count);
	m_pressed.ensure_for(count);
	m_released.ensure_for(count);
}

auto gse::actions::state::set_pressed(const std::uint16_t bit_index, const std::size_t count) -> void {
	ensure_capacity(count);
	m_pressed.set(bit_index);
	m_held.set(bit_index);
}

auto gse::actions::state::set_released(const std::uint16_t bit_index, const std::size_t count) -> void {
	ensure_capacity(count);
	m_released.set(bit_index);
	m_held.clear(bit_index);
}

auto gse::actions::state::set_held(const std::uint16_t bit_index, const bool on, const std::size_t count) -> void {
	ensure_capacity(count);
	if (on) {
		m_held.set(bit_index);
	}
	else {
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

auto gse::actions::state::load_transients(const std::span<const word> pressed, const std::span<const word> released) -> void {
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

auto gse::actions::system::initialize() -> void {
	finalize_bindings();
}

auto gse::actions::system::update() -> void {
	const auto& inputs = system_of<input::system>();
	const auto& in = inputs.current_state();

	auto& s = m_states.write();
	s.begin_frame();
	
	const auto count = m_descriptions.size();
	s.ensure_capacity(count);

	for (auto& [k, bit_index] : m_resolved.key_to_action) {
		if (in.key_pressed(k)) {
			s.set_pressed(bit_index, count);
		}
		if (in.key_released(k)) {
			s.set_released(bit_index, count);
		}
		s.set_held(bit_index, in.key_held(k), count);
	}

	for (auto& [mb, bit_index] : m_resolved.mouse_to_action) {
		if (in.mouse_button_pressed(mb)) {
			s.set_pressed(bit_index, count);
		}
		if (in.mouse_button_released(mb)) {
			s.set_released(bit_index, count);
		}
		s.set_held(bit_index, in.mouse_button_held(mb), count);
	}

	for (const auto& [neg, pos, axis, scale] : m_resolved.axes1_from_keys) {
		const int v = (in.key_held(pos) ? 1 : 0) - (in.key_held(neg) ? 1 : 0);
		s.set_axis1(axis, static_cast<float>(v) * scale);
	}

	for (const auto& [id, left, right, back, fwd, scale] : m_axis2_by_id.items()) {
		const int x = (in.key_held(right) ? 1 : 0) - (in.key_held(left) ? 1 : 0);
		const int y = (in.key_held(back) ? 1 : 0) - (in.key_held(fwd) ? 1 : 0);
		s.set_axis2(static_cast<std::uint16_t>(id.number()), { static_cast<float>(x) * scale, static_cast<float>(y) * scale });
	}

	m_states.flip();

	sample_all_channels(m_states.read());
}

template <gse::actions::fixed_string Tag>
auto gse::actions::system::add(key default_key) -> id {
	auto& desc = add_description(Tag.view());
	m_pending_key_bindings.emplace_back(std::string(Tag.view()), default_key, desc.id());
	return desc.id();
}

auto gse::actions::system::finalize_bindings() -> void {
	m_resolved = {};

	for (const auto& [name, def, action_id] : m_pending_key_bindings) {
		const key k = (m_rebinds.contains(name) ? m_rebinds[name] : def);
		const auto* desc = m_descriptions.try_get(action_id);
		if (!desc) {
			continue;
		}
		m_resolved.key_to_action.emplace_back(k, desc->bit_index());
	}

	m_axis2_by_id.clear();
	auto key_for_action = [&](const id action_id) -> key {
		const auto* desc = m_descriptions.try_get(action_id);
		if (!desc) {
			return key{};
		}
		const auto bit_index = desc->bit_index();
		for (const auto& [k, idx] : m_resolved.key_to_action) {
			if (idx == bit_index) {
				return k;
			}
		}
		return key{};
	};

	for (const auto& [left, right, back, fwd, scale, id] : m_pending_axis2_reqs) {
		resolved_axis2_keys r{
			.id = id,
			.left = key_for_action(left),
			.right = key_for_action(right),
			.back = key_for_action(back),
			.fwd = key_for_action(fwd),
			.scale = scale
		};
		m_axis2_by_id.add(r.id, std::move(r));
	}

	m_axis1_ids_cache.clear();
	for (const auto& k : m_resolved.axes1_from_keys) {
		m_axis1_ids_cache.push_back(k.axis);
	}
	std::ranges::sort(m_axis1_ids_cache);
	m_axis1_ids_cache.erase(std::ranges::unique(m_axis1_ids_cache).begin(), m_axis1_ids_cache.end());

	m_axis2_ids_cache.clear();
	for (const auto& [id, l, r, b, f, s] : m_axis2_by_id.items()) {
		m_axis2_ids_cache.push_back(static_cast<std::uint16_t>(id.number()));
	}
	std::ranges::sort(m_axis2_ids_cache);
	m_axis2_ids_cache.erase(std::ranges::unique(m_axis2_ids_cache).begin(), m_axis2_ids_cache.end());
}

auto gse::actions::system::current_state() const -> const state& {
	return m_states.read();
}

auto gse::actions::system::axis1_ids() const -> std::span<const std::uint16_t> {
	return m_axis1_ids_cache;
}

auto gse::actions::system::axis2_ids() const -> std::span<const std::uint16_t> {
	return m_axis2_ids_cache;
}

auto gse::actions::system::bind_axis2(const pending_axis2_info& info) -> id {
	const id new_id;
	m_pending_axis2_reqs.push_back({ info.left, info.right, info.back, info.fwd, info.scale, new_id });
	return new_id;
}

auto gse::actions::system::register_channel_binding(const id owner, std::function<void(const state&)> sampler) -> void {
	m_channel_bindings.emplace_back(owner, std::move(sampler));
}

auto gse::actions::system::sample_all_channels(const state& s) const -> void {
	for (const auto& [owner, sampler] : m_channel_bindings) {
		sampler(s);
	}
}

auto gse::actions::system::sample_for_entity(const state& s, const id owner_id) const -> void {
	for (const auto& [owner, sampler] : m_channel_bindings) {
		if (owner == owner_id) {
			sampler(s);
		}
	}
}

auto gse::actions::system::description(const id action_id) -> actions::description* {
	return m_descriptions.try_get(action_id);
}

auto gse::actions::system::add_description(const std::string_view tag) -> actions::description& {
	if (const auto existing_id = try_find(tag); existing_id.has_value()) {
		return *m_descriptions.try_get(existing_id.value());
	}

	const auto bit_index = static_cast<std::uint16_t>(m_descriptions.size());
	actions::description desc(std::string(tag), bit_index);
	auto* desc_ptr = m_descriptions.add(desc.id(), std::move(desc));

	return *desc_ptr;
}