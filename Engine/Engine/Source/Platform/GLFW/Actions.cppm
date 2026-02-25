export module gse.platform:actions;

import std;

import gse.utility;
import gse.assert;

import :input;
import :keys;
import :input_state;

export namespace gse {
	auto key_to_string(key k) -> std::string_view;
}

export namespace gse::actions {
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
	class system_state;
	class state;

	class handle {
	public:
		explicit handle(id action_id = {}) : m_action_id(action_id) {}

		auto id() const -> gse::id {
			return m_action_id;
		}

		auto held(
			const state& s,
			system_state& sys
		) const -> bool;

		auto pressed(
			const state& s,
			system_state& sys
		) const -> bool;

		auto released(
			const state& s,
			system_state& sys
		) const -> bool;
	private:
		gse::id m_action_id;
	};

	struct add_action_request {
		std::string name;
		key default_key;
		id action_id;
	};

	struct pending_axis2_info {
		handle left;
		handle right;
		handle back;
		handle fwd;
		float scale = 1.f;
	};

	struct bind_axis2_request {
		pending_axis2_info info;
		id axis_id;
	};

	struct rebind_request {
		std::string action_name;
		key new_key;
	};

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

		auto reset_axes(
			std::span<const std::uint16_t> axes1,
			std::span<const std::uint16_t> axes2
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

		auto load_state(
			std::span<const word> pressed,
			std::span<const word> released,
			std::span<const word> held
		) -> void;

		auto set_camera_yaw(
			float yaw
		) -> void;

		auto camera_yaw(
		) const -> float;
	private:
		auto ensure_axis1_capacity(
			std::uint16_t id
		) -> void;

		auto ensure_axis2_capacity(
			std::uint16_t id
		) -> void;

		mask m_held;
		mask m_pressed;
		mask m_released;

		std::vector<float> m_axes1;
		std::vector<axis> m_axes2;
		float m_camera_yaw = 0.f;
	};

	inline thread_local float g_context_camera_yaw = 0.f;
	inline thread_local bool g_context_camera_yaw_set = false;

	inline auto set_context_camera_yaw(float yaw) -> void {
		g_context_camera_yaw = yaw;
		g_context_camera_yaw_set = true;
	}

	inline auto clear_context_camera_yaw() -> void {
		g_context_camera_yaw_set = false;
	}

	inline auto get_context_camera_yaw() -> std::optional<float> {
		if (g_context_camera_yaw_set) {
			return g_context_camera_yaw;
		}
		return std::nullopt;
	}

	struct button_channel {
		id action_id{};
		bool held = false;
		bool pressed = false;
		bool released = false;

		auto handle() const -> handle {
			return actions::handle(action_id);
		}
	};

	struct axis1_channel {
		std::uint16_t axis_id{};
		float value = 0.f;
	};

	struct axis2_channel {
		id axis_id{};
		axis value{};
	};

	struct action_binding_info {
		std::string name;
		key current_key;
		key default_key;
	};

	struct pending_key_binding {
		std::string name;
		key def;
		id action_id;
	};

	struct pending_axis2_req {
		pending_axis2_info info;
		id axis_id;
	};

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

	struct resolved_axis2_keys {
		id id;
		key left;
		key right;
		key back;
		key fwd;
		float scale = 1.f;
	};

	struct channel_binding {
		id owner;
		std::function<void(const state&)> sampler;
	};

	class system_state {
	public:
		system_state() = default;

		auto current_state(
		) const -> const state&;

		auto axis1_ids(
		) const -> std::span<const std::uint16_t>;

		auto axis2_ids(
		) const -> std::span<const std::uint16_t>;

		auto description(
			id action_id
		) -> actions::description*;

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
		) const -> void;

		auto sample_all_channels(
			const state& s
		) const -> void;

		auto rebinds_map(
		) -> std::map<std::string, int>&;

		[[nodiscard]] auto all_bindings(
		) const -> std::vector<action_binding_info>;

		auto rebind(
			std::string_view action_name,
			key new_key
		) -> void;

		auto finalize_bindings(
		) -> void;

		auto add_description(
			std::string_view tag,
			id action_id
		) -> actions::description&;

		double_buffer<state> states;
		id_mapped_collection<actions::description> descriptions;
		std::vector<pending_key_binding> pending_key_bindings;
		std::map<std::string, int> rebinds;
		std::map<std::string, int> action_defaults;
		std::vector<pending_axis2_req> pending_axis2_reqs;
		bindings resolved;
		std::vector<std::uint16_t> axis1_ids_cache;
		std::vector<std::uint16_t> axis2_ids_cache;
		id_mapped_collection<resolved_axis2_keys> axis2_by_id;
		std::vector<channel_binding> channel_bindings;
	};

	struct system {
		static auto initialize(initialize_phase& phase, system_state& s) -> void;
		static auto update(update_phase& phase, system_state& s) -> void;
		static auto end_frame(end_frame_phase& phase, system_state& s) -> void;
	};
}

auto gse::actions::handle::held(const state& s, system_state& sys) const -> bool {
	if (const auto* desc = sys.description(m_action_id)) {
		return s.held(desc->bit_index());
	}
	return false;
}

auto gse::actions::handle::pressed(const state& s, system_state& sys) const -> bool {
	if (const auto* desc = sys.description(m_action_id)) {
		return s.pressed(desc->bit_index());
	}
	return false;
}

auto gse::actions::handle::released(const state& s, system_state& sys) const -> bool {
	if (const auto* desc = sys.description(m_action_id)) {
		return s.released(desc->bit_index());
	}
	return false;
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

auto gse::actions::state::reset_axes(const std::span<const std::uint16_t> axes1, const std::span<const std::uint16_t> axes2) -> void {
	for (const auto id : axes1) {
		if (id < m_axes1.size()) {
			m_axes1[id] = 0.f;
		}
	}

	for (const auto id : axes2) {
		if (id < m_axes2.size()) {
			m_axes2[id] = {};
		}
	}
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

auto gse::actions::state::ensure_axis1_capacity(const std::uint16_t id) -> void {
	if (id >= m_axes1.size()) {
		m_axes1.resize(static_cast<std::size_t>(id) + 1, 0.f);
	}
}

auto gse::actions::state::ensure_axis2_capacity(const std::uint16_t id) -> void {
	if (id >= m_axes2.size()) {
		m_axes2.resize(static_cast<std::size_t>(id) + 1, {});
	}
}

auto gse::actions::state::set_axis1(const std::uint16_t id, const float v) -> void {
	ensure_axis1_capacity(id);
	m_axes1[id] = v;
}

auto gse::actions::state::set_axis2(const std::uint16_t id, const axis v) -> void {
	ensure_axis2_capacity(id);
	m_axes2[id] = v;
}

auto gse::actions::state::axis1(const std::uint16_t id) const -> float {
	if (id < m_axes1.size()) {
		return m_axes1[id];
	}
	return 0.f;
}

auto gse::actions::state::axis2_v(const std::uint16_t id) const -> axis {
	if (id < m_axes2.size()) {
		return m_axes2[id];
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

auto gse::actions::state::load_state(const std::span<const word> pressed, const std::span<const word> released, const std::span<const word> held) -> void {
	m_pressed.assign(pressed);
	m_released.assign(released);
	m_held.assign(held);
}

auto gse::actions::state::set_camera_yaw(const float yaw) -> void {
	m_camera_yaw = yaw;
}

auto gse::actions::state::camera_yaw() const -> float {
	return m_camera_yaw;
}

auto gse::actions::system::initialize(initialize_phase& phase, system_state& s) -> void {
	phase.channels.push(save::bind_int_map_request{
		.category = "Controls",
		.map_ptr = &s.rebinds
	});

	phase.channels.push(save::bind_int_map_request{
		.category = "ActionDefaults",
		.map_ptr = &s.action_defaults
	});

	s.finalize_bindings();
}

auto gse::actions::system::update(update_phase& phase, system_state& s) -> void {
	bool config_changed = false;

	for (const auto& [name, default_key, action_id] : phase.read_channel<add_action_request>()) {
		s.add_description(name, action_id);
		s.pending_key_bindings.emplace_back(name, default_key, action_id);
		s.action_defaults[name] = static_cast<int>(default_key);
		config_changed = true;
	}

	for (const auto& [info, axis_id] : phase.read_channel<bind_axis2_request>()) {
		s.pending_axis2_reqs.push_back({
			info,
			axis_id
		});
		config_changed = true;
	}

	for (const auto& [action_name, new_key] : phase.read_channel<rebind_request>()) {
		s.rebind(action_name, new_key);
	}

	if (config_changed) {
		s.finalize_bindings();
	}

	const auto* input_state = phase.try_state_of<input::system_state>();
	if (!input_state) {
		return;
	}
	const auto& in = input_state->current_state();

	auto& action_state = s.states.write();
	action_state.begin_frame();

	const auto count = s.descriptions.size();
	action_state.ensure_capacity(count);
	action_state.reset_axes(s.axis1_ids_cache, s.axis2_ids_cache);

	for (auto& [k, bit_index] : s.resolved.key_to_action) {
		if (in.key_pressed(k)) {
			action_state.set_pressed(bit_index, count);
		}
		if (in.key_released(k)) {
			action_state.set_released(bit_index, count);
		}
		action_state.set_held(bit_index, in.key_held(k), count);
	}

	for (auto& [mb, bit_index] : s.resolved.mouse_to_action) {
		if (in.mouse_button_pressed(mb)) {
			action_state.set_pressed(bit_index, count);
		}
		if (in.mouse_button_released(mb)) {
			action_state.set_released(bit_index, count);
		}
		action_state.set_held(bit_index, in.mouse_button_held(mb), count);
	}

	for (const auto& [neg, pos, axis, scale] : s.resolved.axes1_from_keys) {
		const int v = (in.key_held(pos) ? 1 : 0) - (in.key_held(neg) ? 1 : 0);
		action_state.set_axis1(axis, static_cast<float>(v) * scale);
	}

	for (const auto& [id, left, right, back, fwd, scale] : s.axis2_by_id.items()) {
		const int x = (in.key_held(right) ? 1 : 0) - (in.key_held(left) ? 1 : 0);
		const int y = (in.key_held(back) ? 1 : 0) - (in.key_held(fwd) ? 1 : 0);
		action_state.set_axis2(static_cast<std::uint16_t>(id.number()), { static_cast<float>(x) * scale, static_cast<float>(y) * scale });
	}

	for (const auto& desc : s.descriptions.items()) {
		if (const auto idx = desc.bit_index(); action_state.pressed(idx) || action_state.released(idx) || action_state.held(idx)) {
			phase.channels.push(button_channel{ desc.id(), action_state.held(idx), action_state.pressed(idx), action_state.released(idx) });
		}
	}

	for (const auto axis_id : s.axis1_ids_cache) {
		if (const float val = action_state.axis1(axis_id); std::abs(val) > 0.001f) {
			phase.channels.push(axis1_channel{ axis_id, val });
		}
	}

	for (const auto& [id, left, right, back, fwd, scale] : s.axis2_by_id.items()) {
		const auto axis_id = static_cast<std::uint16_t>(id.number());
		if (const auto val = action_state.axis2_v(axis_id); val.x() > 0.001f || val.y() > 0.001f) {
			phase.channels.push(axis2_channel{ id, val });
		}
	}
}

auto gse::actions::system::end_frame(end_frame_phase&, system_state& s) -> void {
	s.states.flip();
}

auto gse::actions::system_state::current_state() const -> const state& {
	return states.read();
}

auto gse::actions::system_state::axis1_ids() const -> std::span<const std::uint16_t> {
	return axis1_ids_cache;
}

auto gse::actions::system_state::axis2_ids() const -> std::span<const std::uint16_t> {
	return axis2_ids_cache;
}

auto gse::actions::system_state::description(const id action_id) -> actions::description* {
	return descriptions.try_get(action_id);
}

auto gse::actions::system_state::register_channel(const id owner_id, button_channel& channel) -> void {
	channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [this, &channel](const state& s) {
			if (const auto* desc = description(channel.action_id)) {
				const auto idx = desc->bit_index();
				channel.held = s.held(idx);
				channel.pressed = s.pressed(idx);
				channel.released = s.released(idx);
			}
			else {
				channel.held = false;
				channel.pressed = false;
				channel.released = false;
			}
		}
	});
}

auto gse::actions::system_state::register_channel(const id owner_id, axis1_channel& channel) -> void {
	channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&channel](const state& s) {
			channel.value = s.axis1(channel.axis_id);
		}
	});
}

auto gse::actions::system_state::register_channel(const id owner_id, axis2_channel& channel) -> void {
	channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&channel](const state& s) {
			channel.value = s.axis2_v(static_cast<std::uint16_t>(channel.axis_id.number()));
		}
	});
}

auto gse::actions::system_state::sample_for_entity(const state& s, const id owner_id) const -> void {
	for (const auto& [owner, sampler] : channel_bindings) {
		if (owner == owner_id) {
			sampler(s);
		}
	}
}

auto gse::actions::system_state::sample_all_channels(const state& s) const -> void {
	for (const auto& [owner, sampler] : channel_bindings) {
		sampler(s);
	}
}

auto gse::actions::system_state::finalize_bindings() -> void {
	resolved = {};

	for (const auto& [name, def, action_id] : pending_key_bindings) {
		const key k = (rebinds.contains(name) ? static_cast<key>(rebinds.at(name)) : def);
		const auto* desc = descriptions.try_get(action_id);
		if (!desc) {
			continue;
		}
		resolved.key_to_action.emplace_back(k, desc->bit_index());
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

	for (const auto& [info, id] : pending_axis2_reqs) {
		resolved_axis2_keys r{
			.id = id,
			.left = key_for_action(info.left.id()),
			.right = key_for_action(info.right.id()),
			.back = key_for_action(info.back.id()),
			.fwd = key_for_action(info.fwd.id()),
			.scale = info.scale
		};
		axis2_by_id.add(r.id, std::move(r));
	}

	axis1_ids_cache.clear();
	for (const auto& k : resolved.axes1_from_keys) {
		axis1_ids_cache.push_back(k.axis);
	}
	std::ranges::sort(axis1_ids_cache);
	axis1_ids_cache.erase(std::ranges::unique(axis1_ids_cache).begin(), axis1_ids_cache.end());

	axis2_ids_cache.clear();
	for (const auto& [id, l, r, b, f, s] : axis2_by_id.items()) {
		axis2_ids_cache.push_back(static_cast<std::uint16_t>(id.number()));
	}
	std::ranges::sort(axis2_ids_cache);
	axis2_ids_cache.erase(std::ranges::unique(axis2_ids_cache).begin(), axis2_ids_cache.end());
}

auto gse::actions::system_state::add_description(const std::string_view tag, const id action_id) -> actions::description& {
	if (const auto existing = descriptions.try_get(action_id)) {
		return *existing;
	}

	const auto bit_index = static_cast<std::uint16_t>(descriptions.size());
	actions::description desc(std::string(tag), bit_index);
	auto* desc_ptr = descriptions.add(action_id, std::move(desc));

	return *desc_ptr;
}

auto gse::actions::system_state::rebinds_map() -> std::map<std::string, int>& {
	return rebinds;
}

auto gse::actions::system_state::all_bindings() const -> std::vector<action_binding_info> {
	std::map<std::string, action_binding_info> merged;

	for (const auto& [name, default_key] : action_defaults) {
		const key def = static_cast<key>(default_key);
		key current = def;
		if (const auto it = rebinds.find(name); it != rebinds.end()) {
			current = static_cast<key>(it->second);
		}
		merged[name] = { name, current, def };
	}

	for (const auto& [name, def, action_id] : pending_key_bindings) {
		key current = def;
		if (const auto it = rebinds.find(name); it != rebinds.end()) {
			current = static_cast<key>(it->second);
		}
		merged[name] = { name, current, def };
	}

	std::vector<action_binding_info> result;
	result.reserve(merged.size());
	for (auto& [name, info] : merged) {
		result.push_back(std::move(info));
	}

	return result;
}

auto gse::actions::system_state::rebind(const std::string_view action_name, const key new_key) -> void {
	rebinds[std::string(action_name)] = static_cast<int>(new_key);
	finalize_bindings();
}

auto gse::key_to_string(const key k) -> std::string_view {
	switch (k) {
		case key::space: return "Space";
		case key::apostrophe: return "'";
		case key::comma: return ",";
		case key::minus: return "-";
		case key::period: return ".";
		case key::slash: return "/";
		case key::num_0: return "0";
		case key::num_1: return "1";
		case key::num_2: return "2";
		case key::num_3: return "3";
		case key::num_4: return "4";
		case key::num_5: return "5";
		case key::num_6: return "6";
		case key::num_7: return "7";
		case key::num_8: return "8";
		case key::num_9: return "9";
		case key::semicolon: return ";";
		case key::equal: return "=";
		case key::a: return "A";
		case key::b: return "B";
		case key::c: return "C";
		case key::d: return "D";
		case key::e: return "E";
		case key::f: return "F";
		case key::g: return "G";
		case key::h: return "H";
		case key::i: return "I";
		case key::j: return "J";
		case key::k: return "K";
		case key::l: return "L";
		case key::m: return "M";
		case key::n: return "N";
		case key::o: return "O";
		case key::p: return "P";
		case key::q: return "Q";
		case key::r: return "R";
		case key::s: return "S";
		case key::t: return "T";
		case key::u: return "U";
		case key::v: return "V";
		case key::w: return "W";
		case key::x: return "X";
		case key::y: return "Y";
		case key::z: return "Z";
		case key::left_bracket: return "[";
		case key::backslash: return "\\";
		case key::right_bracket: return "]";
		case key::grave_accent: return "`";
		case key::escape: return "Escape";
		case key::enter: return "Enter";
		case key::tab: return "Tab";
		case key::backspace: return "Backspace";
		case key::insert: return "Insert";
		case key::del: return "Delete";
		case key::right: return "Right";
		case key::left: return "Left";
		case key::down: return "Down";
		case key::up: return "Up";
		case key::page_up: return "Page Up";
		case key::page_down: return "Page Down";
		case key::home: return "Home";
		case key::end: return "End";
		case key::caps_lock: return "Caps Lock";
		case key::scroll_lock: return "Scroll Lock";
		case key::num_lock: return "Num Lock";
		case key::print_screen: return "Print Screen";
		case key::pause: return "Pause";
		case key::f1: return "F1";
		case key::f2: return "F2";
		case key::f3: return "F3";
		case key::f4: return "F4";
		case key::f5: return "F5";
		case key::f6: return "F6";
		case key::f7: return "F7";
		case key::f8: return "F8";
		case key::f9: return "F9";
		case key::f10: return "F10";
		case key::f11: return "F11";
		case key::f12: return "F12";
		case key::kp_0: return "Numpad 0";
		case key::kp_1: return "Numpad 1";
		case key::kp_2: return "Numpad 2";
		case key::kp_3: return "Numpad 3";
		case key::kp_4: return "Numpad 4";
		case key::kp_5: return "Numpad 5";
		case key::kp_6: return "Numpad 6";
		case key::kp_7: return "Numpad 7";
		case key::kp_8: return "Numpad 8";
		case key::kp_9: return "Numpad 9";
		case key::kp_decimal: return "Numpad .";
		case key::kp_divide: return "Numpad /";
		case key::kp_multiply: return "Numpad *";
		case key::kp_subtract: return "Numpad -";
		case key::kp_add: return "Numpad +";
		case key::kp_enter: return "Numpad Enter";
		case key::left_shift: return "Left Shift";
		case key::left_control: return "Left Ctrl";
		case key::left_alt: return "Left Alt";
		case key::left_super: return "Left Super";
		case key::right_shift: return "Right Shift";
		case key::right_control: return "Right Ctrl";
		case key::right_alt: return "Right Alt";
		case key::right_super: return "Right Super";
		case key::menu: return "Menu";
		default: return "Unknown";
	}
}