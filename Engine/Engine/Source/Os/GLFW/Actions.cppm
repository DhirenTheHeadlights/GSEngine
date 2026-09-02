export module gse.os:actions;

import std;

import gse.core;
import gse.meta;
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
	enum struct key_modifier : std::uint8_t {
		none = 0,
		ctrl = 1 << 0,
		shift = 1 << 1,
		alt = 1 << 2,
		super = 1 << 3
	};

	using key_modifiers = flags<key_modifier>;

	struct key_combo {
		key k = key::unknown;
		key_modifiers mods;
	};

	struct binding {
		actions::binding_source kind = actions::binding_source::key;
		key k = key::unknown;
		mouse_button button = mouse_button::button_1;
		key_modifiers mods;
	};

	auto key_to_string(
		key k
	) -> std::string_view;

	auto mouse_button_to_string(
		mouse_button b
	) -> std::string_view;

	auto is_modifier_key(
		key k
	) -> bool;

	auto combo_to_string(
		key_combo combo
	) -> std::string;

	auto binding_to_string(
		binding b
	) -> std::string;

	auto key_binding(
		key k,
		key_modifiers mods = {}
	) -> binding;

	auto mouse_binding(
		mouse_button b,
		key_modifiers mods = {}
	) -> binding;

	auto same_binding(
		binding a,
		binding b
	) -> bool;

	auto binding_to_config(
		binding b
	) -> std::string;

	auto binding_from_config(
		std::string_view text,
		binding& out
	) -> bool;

	auto bindings_to_config(
		std::span<const binding> list
	) -> std::string;

	auto bindings_from_config(
		std::string_view text,
		std::vector<binding>& out
	) -> bool;
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

	struct rebind_request {
		id action;
		std::vector<binding> new_bindings;
	};

	class description : public identifiable {
	public:
		explicit description(const std::string_view name, const std::uint16_t bit_index)
			: identifiable(std::string(name)), m_bit_index(bit_index) {
		}

		auto bit_index() const -> std::uint16_t {
			return m_bit_index;
		}

		std::string label;
		std::string group;
		bool hidden = false;

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
		id action;
		std::string_view name;
		std::string_view label;
		std::string_view group;
		bool hidden = false;
		std::vector<binding> current;
		std::vector<binding> defaults;
	};

	class registry {
	public:
		auto add(
			std::vector<registration> entries,
			std::vector<axis_registration> axes
		) -> void;

		auto entries() const -> std::span<const registration>;

		auto axes() const -> std::span<const axis_registration>;

		auto revision() const -> std::size_t;

	private:
		std::vector<registration> m_entries;
		std::vector<axis_registration> m_axes;
		std::size_t m_revision = 0;
	};

	struct pending_action {
		std::vector<binding> defaults;
		id action_id;
	};

	struct pending_axis {
		axis_source source = axis_source::digital;
		std::uint8_t dimensions = 2;
		handle neg;
		handle pos;
		handle left;
		handle right;
		handle back;
		handle fwd;
		float scale = 1.f;
		float scale_y = 1.f;
		id axis_id;
	};

	struct bindings {
		struct action_binding {
			binding source;
			std::uint16_t action = 0;
		};
		std::vector<action_binding> to_action;

		struct digital_axis1 {
			std::uint16_t axis;
			std::uint16_t neg_action = 0;
			std::uint16_t pos_action = 0;
			float scale = 1.f;
		};
		std::vector<digital_axis1> axes1_digital;

		struct digital_axis2 {
			std::uint16_t axis;
			std::uint16_t left_action = 0;
			std::uint16_t right_action = 0;
			std::uint16_t back_action = 0;
			std::uint16_t fwd_action = 0;
			float scale = 1.f;
		};
		std::vector<digital_axis2> axes2_digital;

		struct mouse_axis2 {
			std::uint16_t axis;
			float scale_x = 1.f;
			float scale_y = 1.f;
		};
		std::vector<mouse_axis2> axes2_from_mouse;

		struct scroll_axis1 {
			std::uint16_t axis;
			float scale = 1.f;
		};
		std::vector<scroll_axis1> axes1_from_scroll;
	};

	struct [[= system_state<"Actions">{}]] data {
		[[= shared]] state current_input_state;
		[[= shared]] id_mapped_collection<description> descriptions;
		std::vector<pending_action> pending_actions;
		std::map<id, std::vector<binding>> rebinds;
		std::map<id, std::vector<binding>> action_defaults;
		std::vector<pending_axis> pending_axes;
		bindings resolved;
		[[= shared]] std::vector<std::uint16_t> axis1_ids_cache;
		[[= shared]] std::vector<std::uint16_t> axis2_ids_cache;
	};

	auto adopt_declarations(
		data& d,
		const registry& declared
	) -> void;

	auto settings_record(
		data& d,
		settings::draw_page_thunk page = nullptr
	) -> settings::register_settings_type;

	auto push_binding_change(
		settings::change_request_writer channels,
		std::string_view key,
		std::string_view value
	) -> bool;

	[[= system_init{}]] auto init(
		data& d
	) -> async::task<>;

	[[= system_run<>{}]] auto run(
		context& ctx,
		data& d,
		channel_read<rebind_request> requests_in,
		shared_view<input::data> input_s
	) -> async::task<>;

	auto held(
		const state& as,
		shared_view<data> d,
		handle h
	) -> bool;

	auto pressed(
		const state& as,
		shared_view<data> d,
		handle h
	) -> bool;

	auto released(
		const state& as,
		shared_view<data> d,
		handle h
	) -> bool;

	auto current_state(
		shared_view<data> d
	) -> const state&;

	auto axis1_ids(
		shared_view<data> d
	) -> std::span<const std::uint16_t>;

	auto axis2_ids(
		shared_view<data> d
	) -> std::span<const std::uint16_t>;

	auto description_of(
		shared_view<data> d,
		id action_id
	) -> const description*;

	auto rebinds_map(
		data& d
	) -> std::map<id, std::vector<binding>>&;

	[[nodiscard]] auto all_bindings(
		const data& d
	) -> std::vector<action_binding_info>;

	auto log_declared_bindings(
		const data& d
	) -> void;

	auto rebind(
		data& d,
		id action,
		std::vector<binding> new_bindings
	) -> void;

	template <typename KeyHeld>
	auto held_modifiers_from(
		KeyHeld&& key_held
	) -> key_modifiers;

	auto held_modifiers(
		const input::state& in
	) -> key_modifiers;

	auto combo_held(
		const input::state& in,
		key_combo combo
	) -> bool;

	auto binding_held(
		const input::state& in,
		binding b
	) -> bool;

	auto finalize_bindings(
		data& d
	) -> void;

	auto add_description(
		data& d,
		std::string_view tag,
		id action_id
	) -> description&;

	auto held(
		const handle& h,
		const state& s,
		shared_view<data> sys
	) -> bool;

	auto pressed(
		const handle& h,
		const state& s,
		shared_view<data> sys
	) -> bool;

	auto released(
		const handle& h,
		const state& s,
		shared_view<data> sys
	) -> bool;
}

namespace gse::actions {
	using settings_doc = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

	auto write_bindings(
		settings_doc& out,
		std::string_view category,
		const void* settings_ptr,
		settings::scope_kind filter
	) -> void;

	auto read_bindings(
		const settings_doc& in,
		std::string_view category,
		void* settings_ptr,
		settings::scope_kind filter
	) -> void;

	auto apply_reset_rebinds(
		void* settings_ptr
	) -> void;

	auto reset_bindings(
		settings::change_request_writer channels
	) -> void;

	auto modifier_prefix(
		key_modifiers mods
	) -> std::string;
}

auto gse::actions::held(const state& as, const shared_view<data> d, const handle h) -> bool {
	if (const auto* desc = description_of(d, h.id())) {
		return as.held(desc->bit_index());
	}
	return false;
}

auto gse::actions::pressed(const state& as, const shared_view<data> d, const handle h) -> bool {
	if (const auto* desc = description_of(d, h.id())) {
		return as.pressed(desc->bit_index());
	}
	return false;
}

auto gse::actions::released(const state& as, const shared_view<data> d, const handle h) -> bool {
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
	m_held.reset();
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

auto gse::actions::registry::add(std::vector<registration> entries, std::vector<axis_registration> axes) -> void {
	m_entries.insert(m_entries.end(), std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()));
	m_axes.insert(m_axes.end(), std::make_move_iterator(axes.begin()), std::make_move_iterator(axes.end()));
	++m_revision;
}

auto gse::actions::registry::revision() const -> std::size_t {
	return m_revision;
}

auto gse::actions::registry::entries() const -> std::span<const registration> {
	return m_entries;
}

auto gse::actions::registry::axes() const -> std::span<const axis_registration> {
	return m_axes;
}

auto gse::actions::adopt_declarations(data& d, const registry& declared) -> void {
	for (const auto& entry : declared.entries()) {
		const id action_id = generate_id(entry.key);
		if (d.action_defaults.contains(action_id)) {
			continue;
		}

		std::vector<binding> defaults;
		defaults.reserve(entry.bindings.size());
		for (const auto& [kind, code, mods] : entry.bindings) {
			defaults.push_back({
				.kind = kind,
				.k = kind == binding_source::key ? static_cast<key>(code) : key::unknown,
				.button = kind == binding_source::mouse_button ? static_cast<mouse_button>(code) : mouse_button::button_1,
				.mods = key_modifiers::from_bits(mods),
			});
		}

		auto& desc = add_description(d, entry.key, action_id);
		desc.label = entry.label;
		desc.group = entry.group;
		desc.hidden = entry.hidden;

		d.pending_actions.emplace_back(defaults, action_id);
		d.action_defaults[action_id] = std::move(defaults);

		if (entry.handle_ptr) {
			*static_cast<handle*>(entry.handle_ptr) = handle(action_id);
		}
	}

	for (const auto& axis : declared.axes()) {
		const id axis_id = generate_id(axis.key);
		if (std::ranges::contains(d.pending_axes, axis_id, &pending_axis::axis_id)) {
			continue;
		}

		pending_axis pending{
			.source = axis.source,
			.dimensions = axis.dimensions,
			.scale = axis.scale,
			.scale_y = axis.scale_y,
			.axis_id = axis_id,
		};

		if (axis.source == axis_source::digital) {
			if (axis.dimensions == 1) {
				pending.neg = handle(generate_id(axis.neg));
				pending.pos = handle(generate_id(axis.pos));
			}
			else {
				pending.left = handle(generate_id(axis.left));
				pending.right = handle(generate_id(axis.right));
				pending.back = handle(generate_id(axis.back));
				pending.fwd = handle(generate_id(axis.fwd));
			}
		}

		d.pending_axes.push_back(std::move(pending));

		if (axis.axis_id_ptr) {
			*static_cast<id*>(axis.axis_id_ptr) = axis_id;
		}
	}

	finalize_bindings(d);
}

auto gse::actions::init(data& d) -> async::task<> {
	finalize_bindings(d);
	return {};
}

auto gse::actions::write_bindings(settings_doc& out, const std::string_view category, const void* settings_ptr, const settings::scope_kind filter) -> void {
	if (filter != settings::scope_kind::user) {
		return;
	}

	const auto& d = *static_cast<const data*>(settings_ptr);
	for (const auto& [defaults, action_id] : d.pending_actions) {
		const auto it = d.rebinds.find(action_id);
		const auto& active = it != d.rebinds.end() ? it->second : defaults;
		out[std::string(category)][std::string(action_id.tag())] = bindings_to_config(active);
	}
}

auto gse::actions::read_bindings(const settings_doc& in, const std::string_view category, void* settings_ptr, const settings::scope_kind filter) -> void {
	if (filter != settings::scope_kind::user) {
		return;
	}

	const auto category_it = in.find(std::string(category));
	if (category_it == in.end()) {
		return;
	}

	auto& d = *static_cast<data*>(settings_ptr);
	for (const auto& [defaults, action_id] : d.pending_actions) {
		const auto entry = category_it->second.find(std::string(action_id.tag()));
		if (entry == category_it->second.end()) {
			continue;
		}

		std::vector<binding> parsed;
		if (!bindings_from_config(entry->second, parsed)) {
			log::println(
				log::level::warning,
				log::category::general,
				"actions: ignoring unparseable binding for '{}': '{}'",
				action_id.tag(),
				entry->second
			);
			continue;
		}

		const bool matches_default = parsed.size() == defaults.size() &&
			std::ranges::equal(parsed, defaults, same_binding);

		if (matches_default) {
			d.rebinds.erase(action_id);
		}
		else {
			d.rebinds[action_id] = std::move(parsed);
		}
	}
}

auto gse::actions::apply_reset_rebinds(void* settings_ptr) -> void {
	auto& d = *static_cast<data*>(settings_ptr);
	d.rebinds.clear();
	finalize_bindings(d);
}

auto gse::actions::reset_bindings(const settings::change_request_writer channels) -> void {
	channels.push<settings::change_request>({
		.state_type = id_of<data>(),
		.apply = &apply_reset_rebinds,
	});
}

auto gse::actions::push_binding_change(const settings::change_request_writer channels, const std::string_view key, const std::string_view value) -> bool {
	std::vector<binding> parsed;
	if (!bindings_from_config(value, parsed)) {
		return false;
	}

	channels.push<settings::change_request>({
		.state_type = id_of<data>(),
		.apply = [action = generate_id(key), parsed = std::move(parsed)](void* p) {
			auto& d = *static_cast<data*>(p);
			const auto declared = std::ranges::find(d.pending_actions, action, &pending_action::action_id);
			const bool matches_default = declared != d.pending_actions.end() &&
				parsed.size() == declared->defaults.size() &&
				std::ranges::equal(parsed, declared->defaults, same_binding);

			if (matches_default) {
				d.rebinds.erase(action);
			}
			else {
				d.rebinds[action] = parsed;
			}
			finalize_bindings(d);
		},
	});
	return true;
}

auto gse::actions::settings_record(data& d, const settings::draw_page_thunk page) -> settings::register_settings_type {
	std::vector<settings::settings_key_info> keys;
	keys.reserve(d.pending_actions.size());
	for (const auto& [defaults, action_id] : d.pending_actions) {
		keys.push_back({ .key = std::string(action_id.tag()), .scope = settings::scope_kind::user });
	}

	return {
		.category = "Controls",
		.type_id = id_of<data>(),
		.settings_ptr = &d,
		.keys = std::move(keys),
		.write = &write_bindings,
		.read = &read_bindings,
		.reset_to_defaults = &reset_bindings,
		.draw_page = page,
	};
}

auto gse::actions::log_declared_bindings(const data& d) -> void {
	const auto all = all_bindings(d);

	std::map<std::string, std::vector<const action_binding_info*>> by_combo;
	for (const auto& info : all) {
		for (const auto& b : info.current) {
			by_combo[binding_to_string(b)].push_back(&info);
		}
	}

	log::println(
		log::level::info,
		log::category::general,
		"actions: {} declared before first tick",
		all.size()
	);

	for (const auto& info : all) {
		std::string sources;
		for (const auto& b : info.current) {
			if (!sources.empty()) {
				sources += " / ";
			}
			sources += binding_to_string(b);
		}
		log::println(
			log::level::info,
			log::category::general,
			"  {} [{}] = {}{}{}",
			info.label,
			info.name,
			sources.empty() ? "unbound" : sources,
			info.hidden ? " [hidden]" : "",
			info.group.empty() ? "" : std::format(" ({})", info.group)
		);
	}

	for (const auto& [combo, sharers] : by_combo) {
		if (sharers.size() < 2) {
			continue;
		}
		std::string names;
		for (const auto* info : sharers) {
			if (!names.empty()) {
				names += ", ";
			}
			names += info->name;
		}
		log::println(
			log::level::warning,
			log::category::general,
			"actions: {} is bound to {} actions: {}",
			combo,
			sharers.size(),
			names
		);
	}

	log::println(
		log::level::info,
		log::category::general,
		"actions: {} axes declared ({} digital, {} mouse, {} scroll)",
		d.pending_axes.size(),
		d.resolved.axes1_digital.size() + d.resolved.axes2_digital.size(),
		d.resolved.axes2_from_mouse.size(),
		d.resolved.axes1_from_scroll.size()
	);

	for (const auto& axis : d.pending_axes) {
		log::println(
			log::level::info,
			log::category::general,
			"  axis {} [{}] source={} dims={} slot={}",
			axis.axis_id.tag(),
			axis.axis_id.number(),
			enum_to_string(axis.source),
			axis.dimensions,
			static_cast<std::uint16_t>(axis.axis_id.number())
		);
	}
}

auto gse::actions::run(context& ctx, data& d, const channel_read<rebind_request> requests_in, const shared_view<input::data> input_s) -> async::task<> {
	for (const auto& [action, new_bindings] : requests_in.of<rebind_request>()) {
		rebind(d, action, new_bindings);
	}

	const auto& in = input::current_state(input_s);

	auto& action_state = d.current_input_state;
	action_state.begin_frame();

	const auto count = d.descriptions.size();
	action_state.ensure_capacity(count);
	action_state.reset_axes(d.axis1_ids_cache, d.axis2_ids_cache);

	for (const auto& [source, bit_index] : d.resolved.to_action) {
		if (binding_held(in, source)) {
			action_state.set_held(bit_index, true, count);
		}
	}

	action_state.finalize_frame();

	for (const auto& [axis, neg_action, pos_action, scale] : d.resolved.axes1_digital) {
		const int v = (action_state.held(pos_action) ? 1 : 0) - (action_state.held(neg_action) ? 1 : 0);
		action_state.set_axis1(axis, static_cast<float>(v) * scale);
	}

	for (const auto& [axis, left_action, right_action, back_action, fwd_action, scale] : d.resolved.axes2_digital) {
		const int x = (action_state.held(right_action) ? 1 : 0) - (action_state.held(left_action) ? 1 : 0);
		const int y = (action_state.held(back_action) ? 1 : 0) - (action_state.held(fwd_action) ? 1 : 0);
		action_state.set_axis2(axis, { static_cast<float>(x) * scale, static_cast<float>(y) * scale });
	}

	const auto mouse_delta = in.mouse_delta();
	for (const auto& [axis, scale_x, scale_y] : d.resolved.axes2_from_mouse) {
		action_state.set_axis2(axis, { mouse_delta.x() * scale_x, mouse_delta.y() * scale_y });
	}

	const auto scroll = in.scroll_delta();
	for (const auto& [axis, scale] : d.resolved.axes1_from_scroll) {
		action_state.set_axis1(axis, scroll.y() * scale);
	}

	return {};
}

auto gse::actions::current_state(const shared_view<data> d) -> const state& {
	return d.current_input_state;
}

auto gse::actions::axis1_ids(const shared_view<data> d) -> std::span<const std::uint16_t> {
	return d.axis1_ids_cache;
}

auto gse::actions::axis2_ids(const shared_view<data> d) -> std::span<const std::uint16_t> {
	return d.axis2_ids_cache;
}

auto gse::actions::description_of(const shared_view<data> d, const id action_id) -> const description* {
	return d.descriptions.try_get(action_id);
}

auto gse::actions::finalize_bindings(data& d) -> void {
	d.resolved = {};

	for (const auto& [defaults, action_id] : d.pending_actions) {
		const auto* desc = d.descriptions.try_get(action_id);
		if (!desc) {
			continue;
		}
		const auto it = d.rebinds.find(action_id);
		const auto& active = (it != d.rebinds.end() ? it->second : defaults);
		for (const auto& source : active) {
			d.resolved.to_action.push_back({ .source = source, .action = desc->bit_index() });
		}
	}

	const auto bit_of = [&d](const handle h) -> std::uint16_t {
		const auto* desc = d.descriptions.try_get(h.id());
		return desc ? desc->bit_index() : std::uint16_t{ 0 };
	};

	for (const auto& axis : d.pending_axes) {
		const auto axis_bits = static_cast<std::uint16_t>(axis.axis_id.number());
		switch (axis.source) {
			case axis_source::digital:
				if (axis.dimensions == 1) {
					d.resolved.axes1_digital.push_back({
						.axis = axis_bits,
						.neg_action = bit_of(axis.neg),
						.pos_action = bit_of(axis.pos),
						.scale = axis.scale,
					});
				}
				else {
					d.resolved.axes2_digital.push_back({
						.axis = axis_bits,
						.left_action = bit_of(axis.left),
						.right_action = bit_of(axis.right),
						.back_action = bit_of(axis.back),
						.fwd_action = bit_of(axis.fwd),
						.scale = axis.scale,
					});
				}
				break;
			case axis_source::mouse_delta:
				d.resolved.axes2_from_mouse.push_back({
					.axis = axis_bits,
					.scale_x = axis.scale,
					.scale_y = axis.scale_y,
				});
				break;
			case axis_source::scroll:
				d.resolved.axes1_from_scroll.push_back({
					.axis = axis_bits,
					.scale = axis.scale,
				});
				break;
		}
	}

	d.axis1_ids_cache.clear();
	for (const auto& a : d.resolved.axes1_digital) {
		d.axis1_ids_cache.push_back(a.axis);
	}
	for (const auto& a : d.resolved.axes1_from_scroll) {
		d.axis1_ids_cache.push_back(a.axis);
	}
	std::ranges::sort(d.axis1_ids_cache);
	d.axis1_ids_cache.erase(std::ranges::unique(d.axis1_ids_cache).begin(), d.axis1_ids_cache.end());

	d.axis2_ids_cache.clear();
	for (const auto& a : d.resolved.axes2_digital) {
		d.axis2_ids_cache.push_back(a.axis);
	}
	for (const auto& a : d.resolved.axes2_from_mouse) {
		d.axis2_ids_cache.push_back(a.axis);
	}
	std::ranges::sort(d.axis2_ids_cache);
	d.axis2_ids_cache.erase(std::ranges::unique(d.axis2_ids_cache).begin(), d.axis2_ids_cache.end());
}

auto gse::actions::add_description(data& d, const std::string_view tag, const id action_id) -> description& {
	if (const auto existing = d.descriptions.try_get(action_id)) {
		return *existing;
	}

	const auto bit_index = static_cast<std::uint16_t>(d.descriptions.size());
	description desc(std::string(tag), bit_index);
	auto* desc_ptr = d.descriptions.add(action_id, std::move(desc));

	return *desc_ptr;
}

auto gse::actions::rebinds_map(data& d) -> std::map<id, std::vector<binding>>& {
	return d.rebinds;
}

auto gse::actions::all_bindings(const data& d) -> std::vector<action_binding_info> {
	std::vector<action_binding_info> result;
	result.reserve(d.pending_actions.size());

	for (const auto& [defaults, action_id] : d.pending_actions) {
		action_binding_info info{
			.action = action_id,
			.name = action_id.tag(),
			.label = action_id.tag(),
			.current = defaults,
			.defaults = defaults,
		};

		if (const auto it = d.rebinds.find(action_id); it != d.rebinds.end()) {
			info.current = it->second;
		}
		if (const auto* desc = d.descriptions.try_get(action_id)) {
			info.label = desc->label.empty() ? action_id.tag() : desc->label;
			info.group = desc->group;
			info.hidden = desc->hidden;
		}

		result.push_back(std::move(info));
	}

	std::ranges::sort(result, {}, &action_binding_info::name);
	return result;
}

auto gse::actions::rebind(data& d, const id action, std::vector<binding> new_bindings) -> void {
	d.rebinds[action] = std::move(new_bindings);
	finalize_bindings(d);
}

template <typename KeyHeld>
auto gse::actions::held_modifiers_from(KeyHeld&& key_held) -> key_modifiers {
	key_modifiers mods;
	if (key_held(key::left_control) || key_held(key::right_control)) {
		mods.set(key_modifier::ctrl);
	}
	if (key_held(key::left_shift) || key_held(key::right_shift)) {
		mods.set(key_modifier::shift);
	}
	if (key_held(key::left_alt) || key_held(key::right_alt)) {
		mods.set(key_modifier::alt);
	}
	if (key_held(key::left_super) || key_held(key::right_super)) {
		mods.set(key_modifier::super);
	}
	return mods;
}

auto gse::actions::held_modifiers(const input::state& in) -> key_modifiers {
	return held_modifiers_from([&in](const key k) { return in.key_held(k); });
}

auto gse::actions::combo_held(const input::state& in, const key_combo combo) -> bool {
	if (combo.k == key::unknown || !in.key_held(combo.k)) {
		return false;
	}
	if (!combo.mods) {
		return true;
	}
	return held_modifiers(in).bits() == combo.mods.bits();
}

auto gse::actions::binding_held(const input::state& in, const binding b) -> bool {
	if (b.kind == binding_source::mouse_button) {
		if (!in.mouse_button_held(b.button)) {
			return false;
		}
		if (!b.mods) {
			return true;
		}
		return held_modifiers(in).bits() == b.mods.bits();
	}
	return combo_held(in, { .k = b.k, .mods = b.mods });
}

auto gse::actions::held(const handle& h, const state& s, const shared_view<data> sys) -> bool {
	return held(s, sys, h);
}

auto gse::actions::pressed(const handle& h, const state& s, const shared_view<data> sys) -> bool {
	return pressed(s, sys, h);
}

auto gse::actions::released(const handle& h, const state& s, const shared_view<data> sys) -> bool {
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

auto gse::is_modifier_key(const key k) -> bool {
	switch (k) {
		case key::left_control:
		case key::right_control:
		case key::left_shift:
		case key::right_shift:
		case key::left_alt:
		case key::right_alt:
		case key::left_super:
		case key::right_super:
			return true;
		default:
			return false;
	}
}

auto gse::actions::modifier_prefix(const key_modifiers mods) -> std::string {
	std::string text;
	for (const auto mod : enum_values<key_modifier>()) {
		if (mod == key_modifier::none || !mods.test(mod)) {
			continue;
		}
		auto name = std::string(enum_to_string(mod));
		name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
		text += name;
		text += " + ";
	}
	return text;
}

auto gse::combo_to_string(const key_combo combo) -> std::string {
	return actions::modifier_prefix(combo.mods) + std::string(key_to_string(combo.k));
}

auto gse::mouse_button_to_string(const mouse_button b) -> std::string_view {
	switch (b) {
		case mouse_button::button_1:
			return "Left Mouse";
		case mouse_button::button_2:
			return "Right Mouse";
		case mouse_button::button_3:
			return "Middle Mouse";
		case mouse_button::button_4:
			return "Mouse 4";
		case mouse_button::button_5:
			return "Mouse 5";
		case mouse_button::button_6:
			return "Mouse 6";
		case mouse_button::button_7:
			return "Mouse 7";
		case mouse_button::button_8:
			return "Mouse 8";
	}
	return "Mouse Button";
}

auto gse::binding_to_string(const binding b) -> std::string {
	if (b.kind == actions::binding_source::key) {
		return combo_to_string({ .k = b.k, .mods = b.mods });
	}
	return actions::modifier_prefix(b.mods) + std::string(mouse_button_to_string(b.button));
}

auto gse::key_binding(const key k, const key_modifiers mods) -> binding {
	return { .kind = actions::binding_source::key, .k = k, .mods = mods };
}

auto gse::mouse_binding(const mouse_button b, const key_modifiers mods) -> binding {
	return { .kind = actions::binding_source::mouse_button, .button = b, .mods = mods };
}

auto gse::same_binding(const binding a, const binding b) -> bool {
	return a.kind == b.kind && a.k == b.k && a.button == b.button && a.mods.bits() == b.mods.bits();
}

auto gse::binding_to_config(const binding b) -> std::string {
	const bool is_mouse = b.kind == actions::binding_source::mouse_button;

	std::string text = is_mouse ? "mouse:" : "key:";
	for (const auto mod : enum_values<key_modifier>()) {
		if (mod == key_modifier::none || !b.mods.test(mod)) {
			continue;
		}
		text += enum_to_string(mod);
		text += '+';
	}
	text += is_mouse ? enum_to_string(b.button) : enum_to_string(b.k);
	return text;
}

auto gse::binding_from_config(const std::string_view text, binding& out) -> bool {
	const auto colon = text.find(':');
	if (colon == std::string_view::npos) {
		return false;
	}

	binding result;
	const auto kind_text = text.substr(0, colon);
	if (kind_text == "key") {
		result.kind = actions::binding_source::key;
	}
	else if (kind_text == "mouse") {
		result.kind = actions::binding_source::mouse_button;
	}
	else {
		return false;
	}

	auto rest = text.substr(colon + 1);
	for (auto plus = rest.find('+'); plus != std::string_view::npos; plus = rest.find('+')) {
		key_modifier mod{};
		if (!enum_from_string(rest.substr(0, plus), mod) || mod == key_modifier::none) {
			return false;
		}
		result.mods.set(mod);
		rest = rest.substr(plus + 1);
	}

	if (result.kind == actions::binding_source::key) {
		if (!enum_from_string(rest, result.k)) {
			return false;
		}
	}
	else if (!enum_from_string(rest, result.button)) {
		return false;
	}

	out = result;
	return true;
}

auto gse::bindings_to_config(const std::span<const binding> list) -> std::string {
	std::string text;
	for (const auto& b : list) {
		if (!text.empty()) {
			text += ", ";
		}
		text += binding_to_config(b);
	}
	return text;
}

auto gse::bindings_from_config(const std::string_view text, std::vector<binding>& out) -> bool {
	std::vector<binding> parsed;

	std::string_view rest = text;
	while (!rest.empty()) {
		const auto comma = rest.find(',');
		auto token = comma == std::string_view::npos ? rest : rest.substr(0, comma);
		rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);

		while (!token.empty() && token.front() == ' ') {
			token.remove_prefix(1);
		}
		while (!token.empty() && token.back() == ' ') {
			token.remove_suffix(1);
		}
		if (token.empty()) {
			continue;
		}

		binding b;
		if (!binding_from_config(token, b)) {
			return false;
		}
		parsed.push_back(b);
	}

	out = std::move(parsed);
	return true;
}