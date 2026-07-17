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
import :value_widget;
import :text_input_widget;

export namespace gse::settings {
	auto pretty_label(
		std::string_view raw
	) -> std::string;

	struct dimensioned_input_state {
		gui::text_input_state input_state;
		bool initialized = false;
	};

	struct pending_field {
		std::string value;
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
		bool restart_pending_applied = false;

		[[nodiscard]] auto has_pending() const -> bool;
		[[nodiscard]] auto pending_count() const -> std::size_t;
		[[nodiscard]] auto pending_restart_count() const -> std::size_t;
		[[nodiscard]] auto needs_restart() const -> bool;
		auto apply_all(
			channel_writer& channels
		) -> void;
		auto discard_all() -> void;
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

	auto panel(
		gui::builder& b,
		panel_state& ps,
		channel_writer& channels,
		const save::registry& save_reg,
		std::string_view category_filter = ""
	) -> void;

	auto draw_fields_for_entry(
		gui::builder& b,
		panel_state& ps,
		channel_writer& channels,
		const register_settings_type& entry,
		bool hot_only = false
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

auto gse::settings::draw_field_control(gui::builder& b, panel_state& ps, const void* settings_ptr, const settings_field& field, pending_field& pending, const std::uint64_t field_key, const std::string_view display_label) -> void {
	switch (field.widget) {
		case settings_field_widget::boolean: {
			bool value = false;
			gse::parse(pending.value, value);
			b.draw<gui::toggle>({
				.name = display_label,
				.value = value
			});
			pending.value = value ? "true" : "false";
			break;
		}
		case settings_field_widget::choice: {
			const auto options = field.runtime_options ? field.runtime_options(settings_ptr) : field.options;
			if (options.empty()) {
				break;
			}
			int parsed = 0;
			gse::parse(pending.value, parsed);
			std::size_t idx = parsed < 0 ? 0 : static_cast<std::size_t>(parsed);
			if (idx >= options.size()) {
				idx = 0;
			}
			auto& state = ps.dropdowns[field_key];
			const auto r = b.draw<gui::dropdown>({
				.name = display_label,
				.current_index = idx,
				.options = options,
				.state = state,
			});
			if (r.changed) {
				pending.value = std::format("{}", idx);
			}
			break;
		}
		case settings_field_widget::enumeration: {
			if (field.options.empty()) {
				break;
			}
			auto it = std::ranges::find(field.options, pending.value);
			std::size_t idx = it == field.options.end() ? 0 : static_cast<std::size_t>(std::ranges::distance(field.options.begin(), it));
			auto& state = ps.dropdowns[field_key];
			const auto r = b.draw<gui::dropdown>({
				.name = display_label,
				.current_index = idx,
				.options = field.options,
				.state = state,
			});
			if (r.changed && idx < field.options.size()) {
				pending.value = field.options[idx];
			}
			break;
		}
		case settings_field_widget::integer: {
			if (field.range.enabled) {
				int value = 0;
				gse::parse(pending.value, value);
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
				gse::parse(pending.value, value);
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

auto gse::settings::draw_restart_marker(gui::builder& b, const bool modified) -> void {
	if (!b.ctx.current_menu) {
		return;
	}
	auto& ctx = b.ctx;
	const float subline_size = ctx.style.font_size * 0.85f;
	const float subline_height = ctx.font->line_height(subline_size) + ctx.style.padding * 0.25f;
	const gse::rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const gse::rectf subline_rect = gse::rectf::from_position_size(
		{ content_rect.left() + ctx.style.padding, ctx.layout_cursor.y() },
		{ content_rect.width() - ctx.style.padding, subline_height }
	);
	const vec4f badge_color = modified ? ctx.style.color_accent : ctx.style.color_text_secondary;
	ctx.queue_text({
		.font = ctx.font,
		.text = "Requires restart to take effect",
		.position = { subline_rect.left(), subline_rect.center().y() + ctx.font->vertical_center_offset(subline_size) },
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
	const float widget_height = b.ctx.font->line_height(b.ctx.style.font_size) + b.ctx.style.padding * 0.5f;
	const gse::rectf content_rect = b.ctx.current_menu->rect.inset({ b.ctx.style.padding, b.ctx.style.padding });
	const gse::rectf row_rect = gse::rectf::from_position_size(
		{ content_rect.left(), row_y_before },
		{ content_rect.width(), widget_height }
	);
	if (row_rect.contains(b.ctx.input.mouse_position()) && b.ctx.input_available()) {
		const gse::id tooltip_id = gui::ids::make_from_key(hash_combine(field_key, stable_id("##tooltip")));
		b.ctx.set_tooltip(tooltip_id, field.description);
	}
}

auto gse::settings::draw_fields_for_entry(gui::builder& b, panel_state& ps, channel_writer& channels, const register_settings_type& entry, const bool hot_only) -> void {
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

		const std::string display_label = pretty_label(field.key);
		const float row_y_before = b.ctx.current_menu ? b.ctx.layout_cursor.y() : 0.f;
		draw_field_control(b, ps, entry.settings_ptr, field, pending, field_key, display_label);

		pending.modified = pending.value != live_value;
		pending.restart_required = field.restart_required;
		pending.push_change = field.push_change;

		if (field.hot_reloadable) {
			if (pending.modified && pending.push_change) {
				pending.push_change(&channels, pending.value);
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

auto gse::settings::panel_state::apply_all(channel_writer& channels) -> void {
	for (auto& entry : std::views::values(pending_by_type)) {
		for (auto& field : std::views::values(entry.fields)) {
			if (!field.modified || !field.push_change) {
				continue;
			}
			if (field.restart_required) {
				restart_pending_applied = true;
			}
			field.push_change(&channels, field.value);
		}
	}
}

auto gse::settings::panel_state::discard_all() -> void {
	pending_by_type.clear();
	input_buffers.clear();
	input_states.clear();
	for (auto& state : std::views::values(dimensioned_states)) {
		state.initialized = false;
		state.input_state = {};
	}
}

auto gse::settings::pretty_label(const std::string_view raw) -> std::string {
	std::string result;
	result.reserve(raw.size());
	bool capitalize_next = true;
	for (const char c : raw) {
		if (c == '_') {
			result.push_back(' ');
			capitalize_next = true;
		}
		else if (capitalize_next) {
			result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
			capitalize_next = false;
		}
		else {
			result.push_back(c);
		}
	}
	return result;
}

auto gse::settings::panel(gui::builder& b, panel_state& ps, channel_writer& channels, const save::registry& save_reg, const std::string_view category_filter) -> void {
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
								entry.reset_to_defaults(&channels);
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
						entry.draw_page(&sub, &ps, &channels, &entry);
					}
					else if (!entry.fields.empty() && entry.settings_ptr) {
						draw_fields_for_entry(sub, ps, channels, entry);
					}
				});
			}
		);
	}
}
