export module gse.os:actions;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.assert;
import gse.log;

import :input;
import :keys;
import :input_state;

export namespace gse {
	auto key_to_string(key k) -> std::string_view;
}

export namespace gse::actions {
	class handle {
	public:
		handle() = default;
		explicit handle(const id action_id) : m_action_id(action_id) {}

		auto id() const -> id {
			return m_action_id;
		}
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
	using axis = vec2f;

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

		auto clear_all_axes(
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
			angle yaw
		) -> void;

		auto camera_yaw(
		) const -> angle;
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
		angle m_camera_yaw{};
	};

	inline thread_local angle g_context_camera_yaw{};
	inline thread_local bool g_context_camera_yaw_set = false;
	inline thread_local std::unordered_map<id, angle> g_entity_camera_yaw;

	auto set_context_camera_yaw(const angle yaw) -> void {
		g_context_camera_yaw = yaw;
		g_context_camera_yaw_set = true;
	}

	auto clear_context_camera_yaw() -> void {
		g_context_camera_yaw_set = false;
	}

	auto context_camera_yaw() -> std::optional<angle> {
		if (g_context_camera_yaw_set) {
			return g_context_camera_yaw;
		}
		return std::nullopt;
	}

	auto set_entity_camera_yaw(const id entity_id, const angle yaw) -> void {
		g_entity_camera_yaw[entity_id] = yaw;
	}

	auto entity_camera_yaw(const id entity_id) -> std::optional<angle> {
		if (const auto it = g_entity_camera_yaw.find(entity_id); it != g_entity_camera_yaw.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	auto clear_entity_camera_yaw(const id entity_id) -> void {
		g_entity_camera_yaw.erase(entity_id);
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

	struct system {
		struct state {
			actions::state current_input_state;
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

		static auto run(
			run_context& ctx,
			state& s,
			const input::system::state& input_s
		) -> async::task<>;

		static auto held(
			const actions::state& as,
			const state& s,
			handle h
		) -> bool;

		static auto pressed(
			const actions::state& as,
			const state& s,
			handle h
		) -> bool;

		static auto released(
			const actions::state& as,
			const state& s,
			handle h
		) -> bool;

		static auto current_state(
			const state& s
		) -> const actions::state&;

		static auto axis1_ids(
			const state& s
		) -> std::span<const std::uint16_t>;

		static auto axis2_ids(
			const state& s
		) -> std::span<const std::uint16_t>;

		static auto description(
			const state& s,
			id action_id
		) -> const actions::description*;

		static auto register_channel(
			state& s,
			id owner_id,
			button_channel& channel
		) -> void;

		static auto register_channel(
			state& s,
			id owner_id,
			axis1_channel& channel
		) -> void;

		static auto register_channel(
			state& s,
			id owner_id,
			axis2_channel& channel
		) -> void;

		static auto sample_for_entity(
			const state& s,
			const actions::state& as,
			id owner_id
		) -> void;

		static auto sample_all_channels(
			const state& s,
			const actions::state& as
		) -> void;

		static auto rebinds_map(
			state& s
		) -> std::map<std::string, int>&;

		[[nodiscard]] static auto all_bindings(
			const state& s
		) -> std::vector<action_binding_info>;

		static auto rebind(
			state& s,
			std::string_view action_name,
			key new_key
		) -> void;

		static auto finalize_bindings(
			state& s
		) -> void;

		static auto add_description(
			state& s,
			std::string_view tag,
			id action_id
		) -> actions::description&;
	};
}

auto gse::actions::system::held(const actions::state& as, const state& s, const handle h) -> bool {
	if (const auto* desc = description(s, h.id())) {
		return as.held(desc->bit_index());
	}
	return false;
}

auto gse::actions::system::pressed(const actions::state& as, const state& s, const handle h) -> bool {
	if (const auto* desc = description(s, h.id())) {
		return as.pressed(desc->bit_index());
	}
	return false;
}

auto gse::actions::system::released(const actions::state& as, const state& s, const handle h) -> bool {
	if (const auto* desc = description(s, h.id())) {
		return as.released(desc->bit_index());
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

auto gse::actions::state::clear_all_axes() -> void {
	std::ranges::fill(m_axes1, 0.f);
	std::ranges::fill(m_axes2, axis{});
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

auto gse::actions::state::set_camera_yaw(const angle yaw) -> void {
	m_camera_yaw = yaw;
}

auto gse::actions::state::camera_yaw() const -> angle {
	return m_camera_yaw;
}

auto gse::actions::system::run(run_context& ctx, state& s, const input::system::state& input_s) -> async::task<> {
	finalize_bindings(s);

	while (true) {
	bool config_changed = false;

	for (const auto& [name, default_key, action_id] : ctx.read_channel<add_action_request>()) {
		add_description(s, name, action_id);
		s.pending_key_bindings.emplace_back(name, default_key, action_id);
		s.action_defaults[name] = static_cast<int>(default_key);
		config_changed = true;
	}

	for (const auto& [info, axis_id] : ctx.read_channel<bind_axis2_request>()) {
		s.pending_axis2_reqs.push_back({
			info,
			axis_id
		});
		config_changed = true;
	}

	for (const auto& [action_name, new_key] : ctx.read_channel<rebind_request>()) {
		rebind(s, action_name, new_key);
	}

	if (config_changed) {
		finalize_bindings(s);
	}

	const auto& in = input::system::current_state(input_s);

	auto& action_state = s.current_input_state;
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
			ctx.channels.push<button_channel>({ desc.id(), action_state.held(idx), action_state.pressed(idx), action_state.released(idx) });
		}
	}

	for (const auto axis_id : s.axis1_ids_cache) {
		if (const float val = action_state.axis1(axis_id); std::abs(val) > 0.001f) {
			ctx.channels.push<axis1_channel>({ axis_id, val });
		}
	}

	for (const auto& [id, left, right, back, fwd, scale] : s.axis2_by_id.items()) {
		const auto axis_id = static_cast<std::uint16_t>(id.number());
		if (const auto val = action_state.axis2_v(axis_id); val.x() > 0.001f || val.y() > 0.001f) {
			ctx.channels.push<axis2_channel>({ id, val });
		}
	}

		co_await ctx.next_tick();
	}
}

auto gse::actions::system::current_state(const state& s) -> const actions::state& {
	return s.current_input_state;
}

auto gse::actions::system::axis1_ids(const state& s) -> std::span<const std::uint16_t> {
	return s.axis1_ids_cache;
}

auto gse::actions::system::axis2_ids(const state& s) -> std::span<const std::uint16_t> {
	return s.axis2_ids_cache;
}

auto gse::actions::system::description(const state& s, const id action_id) -> const actions::description* {
	return s.descriptions.try_get(action_id);
}

auto gse::actions::system::register_channel(state& s, const id owner_id, button_channel& channel) -> void {
	s.channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&s, &channel](const actions::state& as) {
			if (const auto* desc = description(s, channel.action_id)) {
				const auto idx = desc->bit_index();
				channel.held = as.held(idx);
				channel.pressed = as.pressed(idx);
				channel.released = as.released(idx);
			}
			else {
				channel.held = false;
				channel.pressed = false;
				channel.released = false;
			}
		}
	});
}

auto gse::actions::system::register_channel(state& s, const id owner_id, axis1_channel& channel) -> void {
	s.channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&channel](const actions::state& as) {
			channel.value = as.axis1(channel.axis_id);
		}
	});
}

auto gse::actions::system::register_channel(state& s, const id owner_id, axis2_channel& channel) -> void {
	s.channel_bindings.push_back(channel_binding{
		.owner = owner_id,
		.sampler = [&channel](const actions::state& as) {
			channel.value = as.axis2_v(static_cast<std::uint16_t>(channel.axis_id.number()));
		}
	});
}

auto gse::actions::system::sample_for_entity(const state& s, const actions::state& as, const id owner_id) -> void {
	for (const auto& [owner, sampler] : s.channel_bindings) {
		if (owner == owner_id) {
			sampler(as);
		}
	}
}

auto gse::actions::system::sample_all_channels(const state& s, const actions::state& as) -> void {
	for (const auto& [owner, sampler] : s.channel_bindings) {
		sampler(as);
	}
}

auto gse::actions::system::finalize_bindings(state& s) -> void {
	s.resolved = {};

	for (const auto& [name, def, action_id] : s.pending_key_bindings) {
		const key k = (s.rebinds.contains(name) ? static_cast<key>(s.rebinds.at(name)) : def);
		const auto* desc = s.descriptions.try_get(action_id);
		if (!desc) {
			continue;
		}
		s.resolved.key_to_action.emplace_back(k, desc->bit_index());
	}

	s.axis2_by_id.clear();
	auto key_for_action = [&](const id action_id) -> key {
		const auto* desc = s.descriptions.try_get(action_id);
		if (!desc) {
			return key{};
		}
		const auto bit_index = desc->bit_index();
		for (const auto& [k, idx] : s.resolved.key_to_action) {
			if (idx == bit_index) {
				return k;
			}
		}
		return key{};
	};

	for (const auto& [info, id] : s.pending_axis2_reqs) {
		resolved_axis2_keys r{
			.id = id,
			.left = key_for_action(info.left.id()),
			.right = key_for_action(info.right.id()),
			.back = key_for_action(info.back.id()),
			.fwd = key_for_action(info.fwd.id()),
			.scale = info.scale
		};
		s.axis2_by_id.add(r.id, std::move(r));
	}

	s.axis1_ids_cache.clear();
	for (const auto& k : s.resolved.axes1_from_keys) {
		s.axis1_ids_cache.push_back(k.axis);
	}
	std::ranges::sort(s.axis1_ids_cache);
	s.axis1_ids_cache.erase(std::ranges::unique(s.axis1_ids_cache).begin(), s.axis1_ids_cache.end());

	s.axis2_ids_cache.clear();
	for (const auto& [id, l, r, b, f, sc] : s.axis2_by_id.items()) {
		s.axis2_ids_cache.push_back(static_cast<std::uint16_t>(id.number()));
	}
	std::ranges::sort(s.axis2_ids_cache);
	s.axis2_ids_cache.erase(std::ranges::unique(s.axis2_ids_cache).begin(), s.axis2_ids_cache.end());
}

auto gse::actions::system::add_description(state& s, const std::string_view tag, const id action_id) -> actions::description& {
	if (const auto existing = s.descriptions.try_get(action_id)) {
		return *existing;
	}

	const auto bit_index = static_cast<std::uint16_t>(s.descriptions.size());
	actions::description desc(std::string(tag), bit_index);
	auto* desc_ptr = s.descriptions.add(action_id, std::move(desc));

	return *desc_ptr;
}

auto gse::actions::system::rebinds_map(state& s) -> std::map<std::string, int>& {
	return s.rebinds;
}

auto gse::actions::system::all_bindings(const state& s) -> std::vector<action_binding_info> {
	std::map<std::string, action_binding_info> merged;

	for (const auto& [name, default_key] : s.action_defaults) {
		const auto def = static_cast<key>(default_key);
		key current = def;
		if (const auto it = s.rebinds.find(name); it != s.rebinds.end()) {
			current = static_cast<key>(it->second);
		}
		merged[name] = { name, current, def };
	}

	for (const auto& [name, def, action_id] : s.pending_key_bindings) {
		key current = def;
		if (const auto it = s.rebinds.find(name); it != s.rebinds.end()) {
			current = static_cast<key>(it->second);
		}
		merged[name] = { name, current, def };
	}

	std::vector<action_binding_info> result;
	result.reserve(merged.size());
	for (auto& info : merged | std::views::values) {
		result.push_back(std::move(info));
	}

	return result;
}

auto gse::actions::system::rebind(state& s, const std::string_view action_name, const key new_key) -> void {
	s.rebinds[std::string(action_name)] = static_cast<int>(new_key);
	finalize_bindings(s);
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