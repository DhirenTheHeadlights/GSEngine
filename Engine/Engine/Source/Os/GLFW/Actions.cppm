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
	auto key_to_string(
		key k
	) -> std::string_view;
}

export namespace gse::actions {
	class handle {
	public:
		handle() = default;
		explicit handle(const id action_id) : m_action_id(action_id) {
		}

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
		explicit description(const std::string_view name, const std::uint16_t bit_index)
			: identifiable(name), m_bit_index(bit_index) {
		}

		auto bit_index() const -> std::uint16_t {
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

		auto reset() -> void;

		auto word_count() const -> std::size_t;

		auto words() const -> std::span<const word>;

		auto assign(
			std::span<const word> w
		) -> void;

	private:
		std::vector<word> m_words;
	};

	class state {
	public:
		auto begin_frame() -> void;

		auto finalize_frame() -> void;

		auto ensure_capacity(
			std::size_t count
		) -> void;

		auto reset_axes(
			std::span<const std::uint16_t> axes1,
			std::span<const std::uint16_t> axes2
		) -> void;

		auto clear_all_axes() -> void;

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

		auto held_mask() const -> const mask&;

		auto pressed_mask() const -> const mask&;

		auto released_mask() const -> const mask&;

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

		auto camera_yaw() const -> angle;

	private:
		auto ensure_axis1_capacity(
			std::uint16_t id
		) -> void;

		auto ensure_axis2_capacity(
			std::uint16_t id
		) -> void;

		mask m_held;
		mask m_prev_held;
		mask m_pressed;
		mask m_released;

		std::vector<float> m_axes1;
		std::vector<axis> m_axes2;
		angle m_camera_yaw;
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

	struct [[= gse::system_state<"Actions">{}]] data {
		[[= gse::shared]] actions::state current_input_state;
		[[= gse::shared]] id_mapped_collection<actions::description> descriptions;
		std::vector<pending_key_binding> pending_key_bindings;
		std::map<std::string, int> rebinds;
		std::map<std::string, int> action_defaults;
		std::vector<pending_axis2_req> pending_axis2_reqs;
		bindings resolved;
		[[= gse::shared]] std::vector<std::uint16_t> axis1_ids_cache;
		[[= gse::shared]] std::vector<std::uint16_t> axis2_ids_cache;
		id_mapped_collection<resolved_axis2_keys> axis2_by_id;
	};

	[[= gse::system_init{}]] auto init(
		data& d
	) -> async::task<>;

	[[= gse::system_run<>{}]] auto run(
		context& ctx,
		data& d,
		shared_view<input::data> input_s
	) -> async::task<>;

	auto held(
		const actions::state& as,
		shared_view<data> d,
		handle h
	) -> bool;

	auto pressed(
		const actions::state& as,
		shared_view<data> d,
		handle h
	) -> bool;

	auto released(
		const actions::state& as,
		shared_view<data> d,
		handle h
	) -> bool;

	auto current_state(
		shared_view<data> d
	) -> const actions::state&;

	auto axis1_ids(
		shared_view<data> d
	) -> std::span<const std::uint16_t>;

	auto axis2_ids(
		shared_view<data> d
	) -> std::span<const std::uint16_t>;

	auto description_of(
		shared_view<data> d,
		id action_id
	) -> const actions::description*;

	auto rebinds_map(
		data& d
	) -> std::map<std::string, int>&;

	[[nodiscard]] auto all_bindings(
		const data& d
	) -> std::vector<action_binding_info>;

	auto rebind(
		data& d,
		std::string_view action_name,
		key new_key
	) -> void;

	auto finalize_bindings(
		data& d
	) -> void;

	auto add_description(
		data& d,
		std::string_view tag,
		id action_id
	) -> actions::description&;

	auto add_by_name(
		channel_writer& channels,
		std::string_view tag,
		key default_key
	) -> handle;

	template <fixed_string Tag>
	auto add(
		channel_writer& channels,
		key default_key
	) -> handle;

	auto bind_axis2(
		channel_writer& channels,
		const pending_axis2_info& info,
		id axis_id
	) -> id;

	auto held(
		const handle& h,
		const actions::state& s,
		shared_view<data> sys
	) -> bool;

	auto pressed(
		const handle& h,
		const actions::state& s,
		shared_view<data> sys
	) -> bool;

	auto released(
		const handle& h,
		const actions::state& s,
		shared_view<data> sys
	) -> bool;
}

auto gse::actions::held(const actions::state& as, const shared_view<data> d, const handle h) -> bool {
	if (const auto* desc = description_of(d, h.id())) {
		return as.held(desc->bit_index());
	}
	return false;
}

auto gse::actions::pressed(const actions::state& as, const shared_view<data> d, const handle h) -> bool {
	if (const auto* desc = description_of(d, h.id())) {
		return as.pressed(desc->bit_index());
	}
	return false;
}

auto gse::actions::released(const actions::state& as, const shared_view<data> d, const handle h) -> bool {
	if (const auto* desc = description_of(d, h.id())) {
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
	m_prev_held.assign(m_held.words());
	m_pressed.reset();
	m_released.reset();
}

auto gse::actions::state::finalize_frame() -> void {
	const auto bit_count = m_held.word_count() * 64;
	for (std::uint16_t bit = 0; bit < bit_count; ++bit) {
		const bool h = m_held.test(bit);
		const bool ph = m_prev_held.test(bit);
		if (h && !ph) {
			m_pressed.set(bit);
		}
		else if (!h && ph) {
			m_released.set(bit);
		}
	}
}

auto gse::actions::state::ensure_capacity(const std::size_t count) -> void {
	m_held.ensure_for(count);
	m_prev_held.ensure_for(count);
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
	std::ranges::fill(
		m_axes2,
		axis{}
	);
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
		m_axes2.resize(
			static_cast<std::size_t>(id) + 1,
			{}
		);
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

auto gse::actions::init(data& d) -> async::task<> {
	finalize_bindings(d);
	return {};
}

auto gse::actions::run(context& ctx, data& d, const shared_view<input::data> input_s) -> async::task<> {
	bool config_changed = false;

	for (const auto& [name, default_key, action_id] : ctx.read_channel<add_action_request>()) {
		add_description(d, name, action_id);
		d.pending_key_bindings.emplace_back(name, default_key, action_id);
		d.action_defaults[name] = static_cast<int>(default_key);
		config_changed = true;
	}

	for (const auto& [info, axis_id] : ctx.read_channel<bind_axis2_request>()) {
		d.pending_axis2_reqs.push_back({ info, axis_id });
		config_changed = true;
	}

	for (const auto& [action_name, new_key] : ctx.read_channel<rebind_request>()) {
		rebind(d, action_name, new_key);
	}

	if (config_changed) {
		finalize_bindings(d);
	}

	const auto& in = input::current_state(input_s);

	auto& action_state = d.current_input_state;
	action_state.begin_frame();

	const auto count = d.descriptions.size();
	action_state.ensure_capacity(count);
	action_state.reset_axes(d.axis1_ids_cache, d.axis2_ids_cache);

	for (auto& [k, bit_index] : d.resolved.key_to_action) {
		action_state.set_held(bit_index, in.key_held(k), count);
	}

	for (auto& [mb, bit_index] : d.resolved.mouse_to_action) {
		action_state.set_held(bit_index, in.mouse_button_held(mb), count);
	}

	action_state.finalize_frame();

	for (const auto& [neg, pos, axis, scale] : d.resolved.axes1_from_keys) {
		const int v = (in.key_held(pos) ? 1 : 0) - (in.key_held(neg) ? 1 : 0);
		action_state.set_axis1(axis, static_cast<float>(v) * scale);
	}

	for (const auto& [id, left, right, back, fwd, scale] : d.axis2_by_id.items()) {
		const int x = (in.key_held(right) ? 1 : 0) - (in.key_held(left) ? 1 : 0);
		const int y = (in.key_held(back) ? 1 : 0) - (in.key_held(fwd) ? 1 : 0);
		action_state.set_axis2(
			static_cast<std::uint16_t>(id.number()),
			{ static_cast<float>(x) * scale, static_cast<float>(y) * scale }
		);
	}

	return {};
}

auto gse::actions::current_state(const shared_view<data> d) -> const actions::state& {
	return d.current_input_state;
}

auto gse::actions::axis1_ids(const shared_view<data> d) -> std::span<const std::uint16_t> {
	return d.axis1_ids_cache;
}

auto gse::actions::axis2_ids(const shared_view<data> d) -> std::span<const std::uint16_t> {
	return d.axis2_ids_cache;
}

auto gse::actions::description_of(const shared_view<data> d, const id action_id) -> const actions::description* {
	return d.descriptions.try_get(action_id);
}

auto gse::actions::finalize_bindings(data& d) -> void {
	d.resolved = {};

	for (const auto& [name, def, action_id] : d.pending_key_bindings) {
		const key k = (d.rebinds.contains(name) ? static_cast<key>(d.rebinds.at(name)) : def);
		const auto* desc = d.descriptions.try_get(action_id);
		if (!desc) {
			continue;
		}
		d.resolved.key_to_action.emplace_back(k, desc->bit_index());
	}

	d.axis2_by_id.clear();
	auto key_for_action = [&](const id action_id) -> key {
		const auto* desc = d.descriptions.try_get(action_id);
		if (!desc) {
			return key{};
		}
		const auto bit_index = desc->bit_index();
		for (const auto& [k, idx] : d.resolved.key_to_action) {
			if (idx == bit_index) {
				return k;
			}
		}
		return key{};
	};

	for (const auto& [info, id] : d.pending_axis2_reqs) {
		resolved_axis2_keys r{
			.id = id,
			.left = key_for_action(info.left.id()),
			.right = key_for_action(info.right.id()),
			.back = key_for_action(info.back.id()),
			.fwd = key_for_action(info.fwd.id()),
			.scale = info.scale
		};
		d.axis2_by_id.add(r.id, std::move(r));
	}

	d.axis1_ids_cache.clear();
	for (const auto& k : d.resolved.axes1_from_keys) {
		d.axis1_ids_cache.push_back(k.axis);
	}
	std::ranges::sort(d.axis1_ids_cache);
	d.axis1_ids_cache.erase(std::ranges::unique(d.axis1_ids_cache).begin(), d.axis1_ids_cache.end());

	d.axis2_ids_cache.clear();
	for (const auto& [id, l, r, b, f, sc] : d.axis2_by_id.items()) {
		d.axis2_ids_cache.push_back(static_cast<std::uint16_t>(id.number()));
	}
	std::ranges::sort(d.axis2_ids_cache);
	d.axis2_ids_cache.erase(std::ranges::unique(d.axis2_ids_cache).begin(), d.axis2_ids_cache.end());
}

auto gse::actions::add_description(data& d, const std::string_view tag, const id action_id) -> actions::description& {
	if (const auto existing = d.descriptions.try_get(action_id)) {
		return *existing;
	}

	const auto bit_index = static_cast<std::uint16_t>(d.descriptions.size());
	actions::description desc(std::string(tag), bit_index);
	auto* desc_ptr = d.descriptions.add(action_id, std::move(desc));

	return *desc_ptr;
}

auto gse::actions::rebinds_map(data& d) -> std::map<std::string, int>& {
	return d.rebinds;
}

auto gse::actions::all_bindings(const data& d) -> std::vector<action_binding_info> {
	std::map<std::string, action_binding_info> merged;

	for (const auto& [name, default_key] : d.action_defaults) {
		const auto def = static_cast<key>(default_key);
		key current = def;
		if (const auto it = d.rebinds.find(name); it != d.rebinds.end()) {
			current = static_cast<key>(it->second);
		}
		merged[name] = { name, current, def };
	}

	for (const auto& [name, def, action_id] : d.pending_key_bindings) {
		key current = def;
		if (const auto it = d.rebinds.find(name); it != d.rebinds.end()) {
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

auto gse::actions::rebind(data& d, const std::string_view action_name, const key new_key) -> void {
	d.rebinds[std::string(action_name)] = static_cast<int>(new_key);
	finalize_bindings(d);
}

auto gse::actions::add_by_name(channel_writer& channels, const std::string_view tag, const key default_key) -> handle {
	const id action_id = generate_id(tag);

	channels.push<add_action_request>({
		.name = std::string(tag),
		.default_key = default_key,
		.action_id = action_id,
	});

	return handle(action_id);
}

template <gse::fixed_string Tag>
auto gse::actions::add(channel_writer& channels, const key default_key) -> handle {
	return add_by_name(channels, Tag, default_key);
}

auto gse::actions::bind_axis2(channel_writer& channels, const pending_axis2_info& info, const id axis_id) -> id {
	channels.push<bind_axis2_request>({
		.info = info,
		.axis_id = axis_id,
	});
	return axis_id;
}

auto gse::actions::held(const handle& h, const actions::state& s, const shared_view<data> sys) -> bool {
	return held(s, sys, h);
}

auto gse::actions::pressed(const handle& h, const actions::state& s, const shared_view<data> sys) -> bool {
	return pressed(s, sys, h);
}

auto gse::actions::released(const handle& h, const actions::state& s, const shared_view<data> sys) -> bool {
	return released(s, sys, h);
}

auto gse::key_to_string(const key k) -> std::string_view {
	switch (k) {
		case key::space:
			return "Space";
		case key::apostrophe:
			return "'";
		case key::comma:
			return ",";
		case key::minus:
			return "-";
		case key::period:
			return ".";
		case key::slash:
			return "/";
		case key::num_0:
			return "0";
		case key::num_1:
			return "1";
		case key::num_2:
			return "2";
		case key::num_3:
			return "3";
		case key::num_4:
			return "4";
		case key::num_5:
			return "5";
		case key::num_6:
			return "6";
		case key::num_7:
			return "7";
		case key::num_8:
			return "8";
		case key::num_9:
			return "9";
		case key::semicolon:
			return ";";
		case key::equal:
			return "=";
		case key::a:
			return "A";
		case key::b:
			return "B";
		case key::c:
			return "C";
		case key::d:
			return "D";
		case key::e:
			return "E";
		case key::f:
			return "F";
		case key::g:
			return "G";
		case key::h:
			return "H";
		case key::i:
			return "I";
		case key::j:
			return "J";
		case key::k:
			return "K";
		case key::l:
			return "L";
		case key::m:
			return "M";
		case key::n:
			return "N";
		case key::o:
			return "O";
		case key::p:
			return "P";
		case key::q:
			return "Q";
		case key::r:
			return "R";
		case key::s:
			return "S";
		case key::t:
			return "T";
		case key::u:
			return "U";
		case key::v:
			return "V";
		case key::w:
			return "W";
		case key::x:
			return "X";
		case key::y:
			return "Y";
		case key::z:
			return "Z";
		case key::left_bracket:
			return "[";
		case key::backslash:
			return "\\";
		case key::right_bracket:
			return "]";
		case key::grave_accent:
			return "`";
		case key::escape:
			return "Escape";
		case key::enter:
			return "Enter";
		case key::tab:
			return "Tab";
		case key::backspace:
			return "Backspace";
		case key::insert:
			return "Insert";
		case key::del:
			return "Delete";
		case key::right:
			return "Right";
		case key::left:
			return "Left";
		case key::down:
			return "Down";
		case key::up:
			return "Up";
		case key::page_up:
			return "Page Up";
		case key::page_down:
			return "Page Down";
		case key::home:
			return "Home";
		case key::end:
			return "End";
		case key::caps_lock:
			return "Caps Lock";
		case key::scroll_lock:
			return "Scroll Lock";
		case key::num_lock:
			return "Num Lock";
		case key::print_screen:
			return "Print Screen";
		case key::pause:
			return "Pause";
		case key::f1:
			return "F1";
		case key::f2:
			return "F2";
		case key::f3:
			return "F3";
		case key::f4:
			return "F4";
		case key::f5:
			return "F5";
		case key::f6:
			return "F6";
		case key::f7:
			return "F7";
		case key::f8:
			return "F8";
		case key::f9:
			return "F9";
		case key::f10:
			return "F10";
		case key::f11:
			return "F11";
		case key::f12:
			return "F12";
		case key::kp_0:
			return "Numpad 0";
		case key::kp_1:
			return "Numpad 1";
		case key::kp_2:
			return "Numpad 2";
		case key::kp_3:
			return "Numpad 3";
		case key::kp_4:
			return "Numpad 4";
		case key::kp_5:
			return "Numpad 5";
		case key::kp_6:
			return "Numpad 6";
		case key::kp_7:
			return "Numpad 7";
		case key::kp_8:
			return "Numpad 8";
		case key::kp_9:
			return "Numpad 9";
		case key::kp_decimal:
			return "Numpad .";
		case key::kp_divide:
			return "Numpad /";
		case key::kp_multiply:
			return "Numpad *";
		case key::kp_subtract:
			return "Numpad -";
		case key::kp_add:
			return "Numpad +";
		case key::kp_enter:
			return "Numpad Enter";
		case key::left_shift:
			return "Left Shift";
		case key::left_control:
			return "Left Ctrl";
		case key::left_alt:
			return "Left Alt";
		case key::left_super:
			return "Left Super";
		case key::right_shift:
			return "Right Shift";
		case key::right_control:
			return "Right Ctrl";
		case key::right_alt:
			return "Right Alt";
		case key::right_super:
			return "Right Super";
		case key::menu:
			return "Menu";
		default:
			return "Unknown";
	}
}
