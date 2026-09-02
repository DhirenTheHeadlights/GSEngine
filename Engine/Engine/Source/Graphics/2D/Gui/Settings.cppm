export module gse.graphics:settings;

import std;

import gse.os;
import gse.assets;
import gse.math;
import gse.meta;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.save;

import :types;
import :ids;
import :builder;
import :menu_stack;
import :toggle_widget;
import :slider_widget;
import :dropdown_widget;
import :section_widget;
import :selectable_widget;
import :text_widget;
import :value_widget;
import :text_input_widget;

export namespace gse::settings {
	struct dimensioned_input_state {
		gui::text_input_state input_state;
		gui::dropdown_state dropdown_state;
		std::string magnitude;
		std::string_view unit;
		bool initialized = false;
	};

	struct pending_field {
		std::string value;
		std::string category;
		std::string key;
		push_settings_field_change_thunk push_change = nullptr;
		bool initialized = false;
		bool modified = false;
		bool restart_required = false;
	};

	struct pending_settings {
		std::unordered_map<std::uint64_t, pending_field> fields;
	};

	struct panel_state {
		std::unordered_map<std::uint64_t, gui::dropdown_state> dropdowns;
		std::unordered_map<std::uint64_t, std::string> input_buffers;
		std::unordered_map<std::uint64_t, gui::text_input_state> input_states;
		std::unordered_map<std::uint64_t, dimensioned_input_state> dimensioned_states;
		std::unordered_map<id, pending_settings> pending_by_type;
		std::uint64_t capturing_binding = 0;
		bool restart_pending_applied = false;

		[[nodiscard]] auto has_pending() const -> bool;
		[[nodiscard]] auto pending_count() const -> std::size_t;
		[[nodiscard]] auto pending_restart_count() const -> std::size_t;
		[[nodiscard]] auto needs_restart() const -> bool;
		auto apply_all(
			change_request_writer channels
		) -> void;
		auto discard_all(
			change_request_writer channels
		) -> void;
	};

	using custom_draw_fn = void (
			*
	)(
		gui::builder& b,
		panel_state& ps,
		std::string_view label,
		void* field
	);

	struct draw_with {
		custom_draw_fn fn;
	};

	template <auto Fn>
	struct page_drawer {
		static constexpr auto value = Fn;
	};

	using panel_writer = channel_write<change_request, override_request, gui::popout_toggle>;

	auto panel(
		gui::builder& b,
		panel_state& ps,
		panel_writer channels,
		const save::registry& save_reg,
		std::string_view category_filter = ""
	) -> void;

	auto capture_binding(
		gui::builder& b,
		binding& out
	) -> bool;

	auto draw_controls_page(
		void* builder,
		void* panel_state_ptr,
		change_request_writer channels,
		const void* entry_ptr
	) -> void;

	auto draw_fields_for_entry(
		gui::builder& b,
		panel_state& ps,
		change_request_writer channels,
		const register_settings_type& entry,
		bool hot_only = false,
		const save::registry* save_reg = nullptr
	) -> void;
}

namespace gse::settings {
	auto field_widget_key(
		const register_settings_type& entry,
		const settings_field& field
	) -> std::uint64_t;

	auto draw_field_control(
		gui::builder& b,
		panel_state& ps,
		const void* settings_ptr,
		const settings_field& field,
		pending_field& pending,
		std::uint64_t field_key,
		std::string_view display_label
	) -> void;

	auto draw_dimensioned_field(
		gui::builder& b,
		dimensioned_input_state& state,
		const settings_field& field,
		pending_field& pending,
		std::uint64_t field_key,
		std::string_view display_label
	) -> void;

	auto draw_restart_marker(
		gui::builder& b,
		bool modified
	) -> void;

	auto draw_field_tooltip(
		gui::builder& b,
		const settings_field& field,
		std::uint64_t field_key,
		float row_y_before
	) -> void;
}

auto gse::settings::field_widget_key(const register_settings_type& entry, const settings_field& field) -> std::uint64_t {
	return hash_combine(stable_id(entry.category), stable_id(field.key));
}

auto gse::settings::capture_binding(gui::builder& b, binding& out) -> bool {
	auto& ctx = b.ctx;

	const key_modifiers mods = actions::held_modifiers_from([&ctx](const key k) { return ctx.key_held(k); });

	for (const auto k : enum_values<key>()) {
		if (k == key::unknown || is_modifier_key(k) || k == key::escape) {
			continue;
		}
		if (ctx.key_pressed(k)) {
			ctx.consume_key_press(k);
			out = key_binding(k, mods);
			return true;
		}
	}

	for (const auto button : enum_values<mouse_button>()) {
		if (button == mouse_button::button_1) {
			continue;
		}
		if (ctx.mouse_pressed(button)) {
			ctx.consume_press(button);
			out = mouse_binding(button, mods);
			return true;
		}
	}

	return false;
}

auto gse::settings::draw_controls_page(void* builder, void* panel_state_ptr, const change_request_writer channels, const void* entry_ptr) -> void {
	auto& b = *static_cast<gui::builder*>(builder);
	auto& ps = *static_cast<panel_state*>(panel_state_ptr);
	const auto& entry = *static_cast<const register_settings_type*>(entry_ptr);

	if (!entry.settings_ptr) {
		return;
	}

	const auto& d = *static_cast<const actions::data*>(entry.settings_ptr);
	const auto all = actions::all_bindings(d);
	auto& pending_type = ps.pending_by_type[entry.type_id];

	std::string_view current_group;
	bool any_visible = false;

	for (const auto& info : all) {
		if (info.hidden) {
			continue;
		}
		any_visible = true;

		if (info.group != current_group) {
			current_group = info.group;
			if (!current_group.empty()) {
				b.draw<gui::section>({ .title = current_group });
			}
		}

		const auto row_key = hash_combine(stable_id(entry.category), stable_id(info.name));
		auto& pending = pending_type.fields[row_key];
		const std::string live_value = bindings_to_config(info.current);
		if (!pending.initialized) {
			pending.value = live_value;
			pending.initialized = true;
		}

		const bool capturing = ps.capturing_binding == row_key;

		std::string shown;
		if (capturing) {
			shown = "press a key or mouse button, esc to cancel";
		}
		else {
			std::vector<binding> staged;
			if (bindings_from_config(pending.value, staged) && !staged.empty()) {
				for (const auto& single : staged) {
					if (!shown.empty()) {
						shown += " / ";
					}
					shown += binding_to_string(single);
				}
			}
			else {
				shown = "unbound";
			}
		}

		if (b.draw<gui::selectable>({
				.text = info.label,
				.detail = shown,
				.key = info.name,
				.selected = capturing,
				.align = gui::selectable_align::left,
			})) {
			ps.capturing_binding = capturing ? 0 : row_key;
		}

		if (capturing) {
			if (b.ctx.key_pressed(key::escape)) {
				b.ctx.consume_key_press(key::escape);
				ps.capturing_binding = 0;
			}
			else if (binding captured; capture_binding(b, captured)) {
				pending.value = bindings_to_config({ &captured, 1 });
				ps.capturing_binding = 0;
			}
		}

		pending.modified = pending.value != live_value;
		pending.push_change = &actions::push_binding_change;
		pending.category = entry.category;
		pending.key = info.name;
	}

	if (!any_visible) {
		b.draw<gui::text>({ .content = "No rebindable actions registered." });
	}
}

auto gse::settings::draw_field_control(gui::builder& b, panel_state& ps, const void* settings_ptr, const settings_field& field, pending_field& pending, const std::uint64_t field_key, const std::string_view display_label) -> void {
	switch (field.widget) {
		case settings_field_widget::boolean: {
			bool value = false;
			parse(pending.value, value);
			b.draw<gui::toggle>({
				.name = display_label,
				.value = value
			});
			pending.value = value ? "true" : "false";
			break;
		}
		case settings_field_widget::choice:
		case settings_field_widget::enumeration: {
			const std::vector<settings_field_option> runtime = field.runtime_options
				? field.runtime_options(settings_ptr)
				: std::vector<settings_field_option>{};
			const std::vector<settings_field_option>& options = field.runtime_options ? runtime : field.options;
			if (options.empty()) {
				break;
			}
			const auto it = std::ranges::find(options, pending.value, &settings_field_option::value);
			std::size_t idx = it == options.end() ? 0 : static_cast<std::size_t>(std::ranges::distance(options.begin(), it));
			std::vector<std::string> labels;
			labels.reserve(options.size());
			for (const auto& option : options) {
				labels.push_back(option.label);
			}
			auto& state = ps.dropdowns[field_key];
			const auto r = b.draw<gui::dropdown<>>({
				.name = display_label,
				.current_index = idx,
				.options = labels,
				.state = state,
			});
			if (r.changed && idx < options.size()) {
				pending.value = options[idx].value;
			}
			break;
		}
		case settings_field_widget::integer: {
			if (field.range.enabled) {
				int value = 0;
				parse(pending.value, value);
				b.draw<gui::slider<int>>({
					.name = display_label,
					.value = value,
					.min = static_cast<int>(field.range.min),
					.max = static_cast<int>(field.range.max),
				});
				pending.value = std::format("{}", value);
			}
			else {
				auto& state = ps.input_states[field_key];
				b.draw<gui::text_input>({
					.name = display_label,
					.buffer = pending.value,
					.state = state,
				});
			}
			break;
		}
		case settings_field_widget::floating: {
			if (field.range.enabled) {
				float value = 0.f;
				parse(pending.value, value);
				b.draw<gui::slider<float>>({
					.name = display_label,
					.value = value,
					.min = static_cast<float>(field.range.min),
					.max = static_cast<float>(field.range.max),
				});
				pending.value = std::format("{}", value);
			}
			else {
				auto& state = ps.input_states[field_key];
				b.draw<gui::text_input>({
					.name = display_label,
					.buffer = pending.value,
					.state = state,
				});
			}
			break;
		}
		case settings_field_widget::dimensioned: {
			draw_dimensioned_field(b, ps.dimensioned_states[field_key], field, pending, field_key, display_label);
			break;
		}
		case settings_field_widget::text: {
			auto& state = ps.input_states[field_key];
			b.draw<gui::text_input>({
				.name = display_label,
				.buffer = pending.value,
				.state = state,
			});
			break;
		}
		default:
			break;
	}
}

auto gse::settings::draw_dimensioned_field(gui::builder& b, dimensioned_input_state& state, const settings_field& field, pending_field& pending, const std::uint64_t field_key, const std::string_view display_label) -> void {
	if (field.units.empty() || !field.convert_unit) {
		b.draw<gui::text_input>({
			.name = display_label,
			.buffer = pending.value,
			.state = state.input_state,
		});
		return;
	}

	auto& ctx = b.ctx;
	if (!ctx.current_menu) {
		return;
	}

	if (!state.initialized) {
		state.unit = field.default_unit;
		state.magnitude = field.convert_unit(pending.value, state.unit);
		state.initialized = true;
	}

	const auto text_view = ctx.fonts.text.resolve();
	const float widget_height = text_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const float row_y = ctx.layout_cursor.y();

	float widest_unit = 0.f;
	for (const std::string_view unit : field.units) {
		widest_unit = std::max(widest_unit, text_view->width(unit, ctx.style.font_size));
	}

	const float label_width = content_rect.width() * 0.4f;
	const float unit_width = std::min(content_rect.width() * 0.25f, widest_unit + ctx.style.icon_extent + ctx.style.padding * 2.f);
	const float value_width = std::max(0.f, content_rect.width() - label_width - unit_width - ctx.style.padding * 0.5f);

	const rectf label_rect = rectf::from_position_size(
		{ content_rect.left(), row_y },
		{ label_width, widget_height }
	);
	const rectf value_rect = rectf::from_position_size(
		{ content_rect.left() + label_width, row_y },
		{ value_width, widget_height }
	);
	const rectf unit_rect = rectf::from_position_size(
		{ value_rect.right() + ctx.style.padding * 0.5f, row_y },
		{ unit_width, widget_height }
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = display_label,
		.position = { label_rect.left(), label_rect.center().y() + text_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = label_rect,
	});

	const id input_id = gui::ids::make_from_key(hash_combine(field_key, stable_id("##Magnitude")));

	float slider_min = 0.f;
	float slider_max = 0.f;
	const bool slider_bounds_ready = field.range.enabled
		&& parse(field.convert_unit(std::format("{} {}", field.range.min, field.default_unit), state.unit), slider_min)
		&& parse(field.convert_unit(std::format("{} {}", field.range.max, field.default_unit), state.unit), slider_max)
		&& slider_max > slider_min;

	if (slider_bounds_ready) {
		float magnitude = 0.f;
		parse(state.magnitude, magnitude);
		gui::draw::slider_in_rect(ctx, value_rect, input_id, magnitude, slider_min, slider_max, b.hot_widget_id, b.active_widget_id, state.magnitude);
		state.magnitude = std::format("{}", magnitude);
	}
	else {
		gui::draw::text_input_in_rect(ctx, input_id, state.magnitude, state.input_state, value_rect, b.hot_widget_id, b.focus_widget_id);
	}

	const auto selected = std::ranges::find(field.units, state.unit);
	const gui::dropdown_result picked = gui::draw::dropdown_in_rect_keyed(
		ctx,
		hash_combine(field_key, stable_id("##Unit")),
		selected == field.units.end() ? 0 : static_cast<std::size_t>(std::ranges::distance(field.units.begin(), selected)),
		field.units,
		state.dropdown_state,
		unit_rect,
		b.hot_widget_id,
		b.active_widget_id
	);
	if (picked.changed && picked.new_index < field.units.size()) {
		state.unit = field.units[picked.new_index];
		if (std::string converted = field.convert_unit(pending.value, state.unit); !converted.empty()) {
			state.magnitude = std::move(converted);
		}
	}

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;

	if (state.magnitude.empty() || !field.normalize) {
		return;
	}
	if (std::string canonical = field.normalize(std::format("{} {}", state.magnitude, state.unit)); !canonical.empty()) {
		pending.value = std::move(canonical);
	}
}

auto gse::settings::draw_restart_marker(gui::builder& b, const bool modified) -> void {
	if (!b.ctx.current_menu) {
		return;
	}
	auto& ctx = b.ctx;
	const float subline_size = ctx.style.font_size * 0.85f;
	const auto text_view = ctx.fonts.text.resolve();
	const float subline_height = text_view->line_height(subline_size) + ctx.style.padding * 0.25f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const rectf subline_rect = rectf::from_position_size(
		{ content_rect.left() + ctx.style.padding, ctx.layout_cursor.y() },
		{ content_rect.width() - ctx.style.padding, subline_height }
	);
	const vec4f badge_color = modified ? ctx.style.color_accent : ctx.style.color_text_secondary;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = "Requires restart to take effect",
		.position = { subline_rect.left(), subline_rect.center().y() + text_view->vertical_center_offset(subline_size) },
		.scale = subline_size,
		.color = badge_color,
		.clip_rect = subline_rect,
	});
	ctx.layout_cursor.y() -= subline_height;
}

auto gse::settings::draw_field_tooltip(gui::builder& b, const settings_field& field, const std::uint64_t field_key, const float row_y_before) -> void {
	if (field.description.empty() || !b.ctx.current_menu) {
		return;
	}
	const float widget_height = b.ctx.fonts.text.resolve()->line_height(b.ctx.style.font_size) + b.ctx.style.padding * 0.5f;
	const rectf content_rect = b.ctx.current_menu->rect.inset({ b.ctx.style.padding, b.ctx.style.padding });
	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), row_y_before },
		{ content_rect.width(), widget_height }
	);
	if (b.ctx.hovers(row_rect)) {
		const id tooltip_id = gui::ids::make_from_key(hash_combine(field_key, stable_id("##tooltip")));
		b.ctx.set_tooltip(tooltip_id, field.description);
	}
}

auto gse::settings::draw_fields_for_entry(gui::builder& b, panel_state& ps, const change_request_writer channels, const register_settings_type& entry, const bool hot_only, const save::registry* save_reg) -> void {
	if (!entry.settings_ptr) {
		return;
	}
	auto& pending_type = ps.pending_by_type[entry.type_id];
	for (const settings_field& field : entry.fields) {
		if (hot_only && !field.hot_reloadable) {
			continue;
		}
		const std::uint64_t field_key = field_widget_key(entry, field);
		auto& pending = pending_type.fields[field_key];
		const std::string live_value = field.format ? field.format(entry.settings_ptr) : std::string{};
		if (!pending.initialized || field.hot_reloadable) {
			pending.value = live_value;
			pending.initialized = true;
		}

		const bool session_pinned = save_reg && save_reg->provenance_of(entry.category, field.key) == save::value_provenance::session;
		const std::string display_label = session_pinned ? std::format("{} [session]", pretty_label(field.key)) : pretty_label(field.key);
		const float row_y_before = b.ctx.current_menu ? b.ctx.layout_cursor.y() : 0.f;
		draw_field_control(b, ps, entry.settings_ptr, field, pending, field_key, display_label);

		pending.modified = pending.value != live_value;
		pending.restart_required = field.restart_required;
		pending.push_change = field.push_change;
		pending.category = entry.category;
		pending.key = field.key;

		if (field.hot_reloadable && !field.restart_required) {
			if (pending.modified && pending.push_change) {
				pending.push_change(channels, pending.key, pending.value);
				channels.push<override_request>({
					.op = override_op::release_override,
					.category = pending.category,
					.key = pending.key
				});
			}
			pending.modified = false;
		}

		if (field.restart_required) {
			draw_restart_marker(b, pending.modified);
		}
		draw_field_tooltip(b, field, field_key, row_y_before);
	}
}

auto gse::settings::panel_state::has_pending() const -> bool {
	return pending_count() > 0;
}

auto gse::settings::panel_state::pending_count() const -> std::size_t {
	std::size_t total = 0;
	for (const auto& entry : std::views::values(pending_by_type)) {
		for (const auto& field : std::views::values(entry.fields)) {
			if (field.modified) {
				++total;
			}
		}
	}
	return total;
}

auto gse::settings::panel_state::pending_restart_count() const -> std::size_t {
	std::size_t total = 0;
	for (const auto& entry : std::views::values(pending_by_type)) {
		for (const auto& field : std::views::values(entry.fields)) {
			if (field.modified && field.restart_required) {
				++total;
			}
		}
	}
	return total;
}

auto gse::settings::panel_state::needs_restart() const -> bool {
	return restart_pending_applied;
}

auto gse::settings::panel_state::apply_all(const change_request_writer channels) -> void {
	for (auto& entry : std::views::values(pending_by_type)) {
		for (auto& field : std::views::values(entry.fields)) {
			if (field.restart_required) {
				if (!field.category.empty()) {
					if (field.modified) {
						channels.push<override_request>({
							.op = override_op::stage_value,
							.category = field.category,
							.key = field.key,
							.value = field.value
						});
						restart_pending_applied = true;
					}
					else {
						channels.push<override_request>({
							.op = override_op::clear_staged,
							.category = field.category,
							.key = field.key
						});
					}
				}
				continue;
			}
			if (!field.modified || !field.push_change) {
				continue;
			}
			field.push_change(channels, field.key, field.value);
			if (!field.category.empty()) {
				channels.push<override_request>({
					.op = override_op::release_override,
					.category = field.category,
					.key = field.key
				});
			}
		}
	}
}

auto gse::settings::panel_state::discard_all(const change_request_writer channels) -> void {
	for (const auto& entry : std::views::values(pending_by_type)) {
		for (const auto& field : std::views::values(entry.fields)) {
			if (field.restart_required && !field.category.empty()) {
				channels.push<override_request>({
					.op = override_op::clear_staged,
					.category = field.category,
					.key = field.key
				});
			}
		}
	}

	pending_by_type.clear();
	capturing_binding = 0;
	input_buffers.clear();
	input_states.clear();
	for (auto& state : std::views::values(dimensioned_states)) {
		state.initialized = false;
		state.input_state = {};
	}
}

auto gse::settings::panel(gui::builder& b, panel_state& ps, const panel_writer channels, const save::registry& save_reg, const std::string_view category_filter) -> void {
	std::vector<std::string> category_order;
	std::unordered_set<std::string> seen;
	save_reg.for_each_entry([&](const register_settings_type& entry) {
		if (!category_filter.empty() && entry.category != category_filter) {
			return;
		}
		if (entry.fields.empty() && !entry.draw_page) {
			return;
		}
		if (entry.category.empty()) {
			return;
		}
		if (seen.insert(entry.category).second) {
			category_order.push_back(entry.category);
		}
	});
	std::ranges::sort(category_order);

	for (const auto& cat : category_order) {
		bool category_has_hot = false;
		save_reg.for_each_entry([&](const register_settings_type& entry) {
			if (entry.category == cat && std::ranges::any_of(entry.fields, &settings_field::hot_reloadable)) {
				category_has_hot = true;
			}
		});

		b.scroll_region(
			{
				.id = cat
			},
			[&](gui::builder& sub) {
				sub.draw<gui::section>({
					.title = cat,
					.action_icon = category_has_hot ? std::string_view("\xE2\x86\x97 Live") : std::string_view{},
					.on_action = category_has_hot ? std::function<void()>([&channels, cat] {
						channels.push<gui::popout_toggle>({
							.category = cat
						});
					})
												  : std::function<void()>{},
					.secondary_action_icon = "\xE2\x86\xBA Reset",
					.on_secondary_action = [&channels, &save_reg, &ps, cat] {
						save_reg.for_each_entry([&](const register_settings_type& entry) {
							if (entry.category != cat) {
								return;
							}
							if (entry.reset_to_defaults) {
								entry.reset_to_defaults(channels);
							}
							ps.pending_by_type.erase(entry.type_id);
						});
						ps.input_buffers.clear();
						ps.input_states.clear();
						for (auto& state : std::views::values(ps.dimensioned_states)) {
							state.initialized = false;
							state.input_state = {};
						}
					},
				});
				save_reg.for_each_entry([&](const register_settings_type& entry) {
					if (entry.category != cat) {
						return;
					}
					if (entry.draw_page && entry.settings_ptr) {
						entry.draw_page(&sub, &ps, channels, &entry);
					}
					else if (!entry.fields.empty() && entry.settings_ptr) {
						draw_fields_for_entry(sub, ps, channels, entry, false, &save_reg);
					}
				});
			}
		);
	}
}