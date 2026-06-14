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

	struct pending_state_base {
		virtual ~pending_state_base() = default;
		[[nodiscard]] virtual auto modified_count() const -> std::size_t = 0;
		[[nodiscard]] virtual auto restart_count() const -> std::size_t = 0;
		virtual auto push(
			channel_writer& channels
		) -> void = 0;
	};

	template <typename S>
	struct pending_state : pending_state_base {
		using data_t = typename S::data;
		data_t value{};
		std::unordered_map<std::uint64_t, std::function<void(data_t&)>> apply_fns;
		std::unordered_set<std::uint64_t> restart_fields;
		bool initialized = false;

		[[nodiscard]] auto modified_count() const -> std::size_t override;

		[[nodiscard]] auto restart_count() const -> std::size_t override;

		auto push(
			channel_writer& channels
		) -> void override;
	};

	struct panel_state {
		std::unordered_map<std::uint64_t, gui::dropdown_state> dropdowns;
		std::unordered_map<std::uint64_t, std::string> input_buffers;
		std::unordered_map<std::uint64_t, gui::text_input_state> input_states;
		std::unordered_map<std::uint64_t, dimensioned_input_state> dimensioned_states;
		std::unordered_map<id, std::unique_ptr<pending_state_base>> pending_by_type;
		bool restart_pending_applied = false;

		template <typename S>
		auto pending_for(
			const typename S::data& live
		) -> pending_state<S>&;

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

	template <has_settings S, bool HotOnly = false>
	auto draw_fields(
		gui::builder& b,
		panel_state& ps,
		const typename S::data& live,
		channel_writer& channels
	) -> void;

	template <typename S>
	auto draw_struct_thunk(
		void* gui_builder,
		void* panel_state,
		std::string_view category,
		void* settings_ptr,
		void* channels_writer
	) -> void;

	template <has_settings S>
	auto draw_page_thunk_impl(
		void* gui_builder,
		void* panel_state,
		void* settings_ptr,
		void* channels_writer
	) -> void;

	template <has_settings S>
	auto draw_hot_fields_thunk_impl(
		void* gui_builder,
		void* panel_state,
		void* settings_ptr,
		void* channels_writer
	) -> void;

	template <typename T>
	consteval auto has_page_drawer() -> bool {
		return meta::find_class_template_annotation(^^T, ^^page_drawer) != std::meta::info{};
	}

	template <typename T>
	consteval auto has_hot_reloadable_fields() -> bool {
		for (auto m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
			if (has_annotation<settings::hot_reloadable_tag>(m)) {
				return true;
			}
		}
		return false;
	}
}

export namespace gse::settings {
	template <has_settings S>
	struct gui_draw_provider<S> {
		static constexpr draw_settings_thunk value = &draw_struct_thunk<S>;
		static constexpr draw_page_thunk page_value =
			has_page_drawer<typename S::data>() ? &draw_page_thunk_impl<S> : nullptr;
		static constexpr draw_hot_fields_thunk hot_value =
			has_hot_reloadable_fields<typename S::data>() ? &draw_hot_fields_thunk_impl<S> : nullptr;
		static constexpr bool any_hot = has_hot_reloadable_fields<typename S::data>();
	};
}

template <typename S>
auto gse::settings::pending_state<S>::modified_count() const -> std::size_t {
	return apply_fns.size();
}

template <typename S>
auto gse::settings::pending_state<S>::restart_count() const -> std::size_t {
	return restart_fields.size();
}

template <typename S>
auto gse::settings::pending_state<S>::push(channel_writer& channels) -> void {
	if (apply_fns.empty()) {
		return;
	}
	std::vector<std::function<void(data_t&)>> applies;
	applies.reserve(apply_fns.size());
	for (const auto& fn : std::views::values(apply_fns)) {
		applies.push_back(fn);
	}
	channels.push<change_request<S>>({
		.apply = [applies = std::move(applies)](data_t& d) {
			for (const auto& fn : applies) {
				fn(d);
			}
		},
	});
}

template <typename S>
auto gse::settings::panel_state::pending_for(const typename S::data& live) -> pending_state<S>& {
	using data_t = typename S::data;
	const id type_key = id_of<S>();
	auto it = pending_by_type.find(type_key);
	if (it == pending_by_type.end()) {
		auto entry = std::make_unique<pending_state<S>>();
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^data_t, std::meta::access_context::unchecked()))) {
			if constexpr (meta::find_describe(m) != std::meta::info{}) {
				entry->value.[:m:] = live.[:m:];
			}
		}
		entry->initialized = true;
		it = pending_by_type.emplace(type_key, std::move(entry)).first;
	}
	return static_cast<pending_state<S>&>(*it->second);
}

namespace gse::settings {
	template <typename E>
	auto draw_enum_dropdown(
		gui::builder& b,
		panel_state& ps,
		std::uint64_t key,
		std::string_view label,
		E& ref
	) -> void;
}

template <typename E>
auto gse::settings::draw_enum_dropdown(gui::builder& b, panel_state& ps, const std::uint64_t key, const std::string_view label, E& ref) -> void {
	static const std::vector<std::string> options = [] {
		std::vector<std::string> v;
		template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
			v.emplace_back(std::meta::identifier_of(e));
		}
		return v;
	}();

	static const std::vector<E> values = [] {
		std::vector<E> v;
		template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
			v.push_back([:e:]);
		}
		return v;
	}();

	auto& dd_state = ps.dropdowns[key];

	std::size_t idx = 0;
	for (std::size_t i = 0; i < values.size(); ++i) {
		if (values[i] == ref) {
			idx = i;
			break;
		}
	}

	const auto r = b.draw<gui::dropdown>({
		.name = label,
		.current_index = idx,
		.options = options,
		.state = dd_state,
	});

	if (r.changed && idx < values.size()) {
		ref = values[idx];
	}
}

template <typename S>
auto gse::settings::draw_struct_thunk(void* gui_builder, void* panel_state_ptr, const std::string_view category, void* settings_ptr, void* channels_writer) -> void {
	(void)category;
	using data_t = typename S::data;
	auto& b = *static_cast<gui::builder*>(gui_builder);
	auto& ps = *static_cast<panel_state*>(panel_state_ptr);
	const auto& live = *static_cast<const data_t*>(settings_ptr);
	auto& channels = *static_cast<channel_writer*>(channels_writer);
	draw_fields<S>(b, ps, live, channels);
}

template <gse::has_settings S>
auto gse::settings::draw_page_thunk_impl(void* gui_builder, void* panel_state_ptr, void* settings_ptr, void* channels_writer) -> void {
	using data_t = typename S::data;
	auto& b = *static_cast<gui::builder*>(gui_builder);
	auto& ps = *static_cast<panel_state*>(panel_state_ptr);
	const auto& live = *static_cast<const data_t*>(settings_ptr);
	auto& channels = *static_cast<channel_writer*>(channels_writer);

	auto& pending = ps.template pending_for<S>(live).value;

	if constexpr (has_page_drawer<data_t>()) {
		constexpr auto ann = meta::find_class_template_annotation(^^data_t, ^^page_drawer);
		using drawer_t = [:ann:];
		constexpr auto fn = drawer_t::value;
		fn(b, ps, live, pending, channels);
	}
}

template <gse::has_settings S>
auto gse::settings::draw_hot_fields_thunk_impl(void* gui_builder, void* panel_state_ptr, void* settings_ptr, void* channels_writer) -> void {
	using data_t = typename S::data;
	auto& b = *static_cast<gui::builder*>(gui_builder);
	auto& ps = *static_cast<panel_state*>(panel_state_ptr);
	const auto& live = *static_cast<const data_t*>(settings_ptr);
	auto& channels = *static_cast<channel_writer*>(channels_writer);

	draw_fields<S, true>(b, ps, live, channels);
}

namespace gse::settings {
	template <has_settings S, auto M, typename F, typename Apply>
	auto draw_one(
		gui::builder& b,
		panel_state& ps,
		pending_state<S>& pend,
		channel_writer& channels,
		std::uint64_t category_hash,
		std::uint64_t field_key,
		std::string_view display_label,
		F& local_value,
		const F& live_value,
		Apply apply
	) -> void;
}

template <gse::has_settings S, auto M, typename F, typename Apply>
auto gse::settings::draw_one(gui::builder& b, panel_state& ps, pending_state<S>& pend, channel_writer& channels, const std::uint64_t category_hash, const std::uint64_t field_key, const std::string_view display_label, F& local_value, const F& live_value, Apply apply) -> void {
	using data_t = typename S::data;
	using describe_t = [:meta::find_describe(M):];
	constexpr std::string_view describe_text = describe_t::value;
	constexpr bool field_needs_restart = has_annotation<settings::restart_required>(M);
	constexpr std::uint64_t tooltip_suffix_hash = hash_combine(stable_id(meta::member_name(M)), stable_id("##tooltip"));

	const float row_y_before = b.ctx.current_menu ? b.ctx.layout_cursor.y() : 0.f;

	if constexpr (has_annotation<draw_with>(M)) {
		constexpr auto dw = annotation_of<draw_with, M>();
		dw.fn(b, ps, display_label, &local_value);
	}
	else if constexpr (std::same_as<F, bool>) {
		b.draw<gui::toggle>({
			.name = display_label,
			.value = local_value
		});
	}
	else if constexpr (settings::is_choice_v<F>) {
		auto& dd_state = ps.dropdowns[field_key];
		std::size_t idx = static_cast<std::size_t>(local_value.value);
		const auto r = b.draw<gui::dropdown>({
			.name = display_label,
			.current_index = idx,
			.options = local_value.options,
			.state = dd_state,
		});
		if (r.changed) {
			local_value.value = static_cast<typename F::value_type>(idx);
		}
	}
	else if constexpr (std::is_enum_v<F>) {
		draw_enum_dropdown<F>(b, ps, field_key, display_label, local_value);
	}
	else if constexpr (constexpr auto range_t = meta::find_range(M); range_t != std::meta::info{}) {
		using R = [:range_t:];
		if constexpr (gse::internal::is_quantity<F>) {
			F min_q;
			F max_q;
			if constexpr (std::same_as<std::remove_cvref_t<decltype(R::min)>, F>) {
				min_q = R::min;
				max_q = R::max;
			}
			else {
				using underlying = typename F::value_type;
				min_q = F::template from<typename F::default_unit>(static_cast<underlying>(R::min));
				max_q = F::template from<typename F::default_unit>(static_cast<underlying>(R::max));
			}
			b.draw<gui::quantity_slider<F, typename F::default_unit{}>>({
				.name = display_label,
				.value = local_value,
				.min = min_q,
				.max = max_q,
			});
		}
		else {
			b.draw<gui::slider<F>>({
				.name = display_label,
				.value = local_value,
				.min = static_cast<F>(R::min),
				.max = static_cast<F>(R::max),
			});
		}
	}
	else if constexpr (gse::internal::is_quantity<F>) {
		auto& dim_state = ps.dimensioned_states[field_key];
		auto& buffer = ps.input_buffers[field_key];
		using unit_t = typename F::default_unit;

		if (!dim_state.initialized) {
			buffer = std::format("{}", gse::internal::value_in<unit_t>(local_value));
			dim_state.initialized = true;
		}

		b.draw<gui::text_input>({
			.name = display_label,
			.buffer = buffer,
			.state = dim_state.input_state,
		});

		typename F::value_type typed_value{};
		if (gse::parse(buffer, typed_value)) {
			local_value = F::template from<unit_t>(typed_value);
		}
	}
	else if constexpr (gse::is_arithmetic<F>) {
		auto it = ps.input_buffers.find(field_key);
		if (it == ps.input_buffers.end()) {
			std::string init;
			std::format_to(std::back_inserter(init), "{}", local_value);
			it = ps.input_buffers.emplace(field_key, std::move(init)).first;
		}
		auto& buffer = it->second;

		auto& state = ps.input_states[field_key];

		b.draw<gui::text_input>({
			.name = display_label,
			.buffer = buffer,
			.state = state,
		});

		F parsed{};
		if (gse::parse(buffer, parsed)) {
			local_value = parsed;
		}
	}
	else if constexpr (gse::is_vec<F>) {
		using elem_t = typename F::value_type;
		if constexpr (gse::is_arithmetic<elem_t>) {
			b.draw<gui::vec_value<elem_t, F::extent>>({
				.name = display_label,
				.val = local_value,
			});
		}
		else if constexpr (gse::internal::is_quantity<elem_t>) {
			b.draw<gui::quantity_vec_value<elem_t, F::extent>>({
				.name = display_label,
				.val = local_value,
			});
		}
	}

	const bool is_modified = [&]() -> bool {
		if constexpr (settings::is_choice_v<F>) {
			return local_value.value != live_value.value;
		}
		else if constexpr (requires(F a, F b_) { a == b_; }) {
			return local_value != live_value;
		}
		else {
			return false;
		}
	}();

	if (is_modified) {
		if constexpr (has_annotation<settings::hot_reloadable_tag>(M)) {
			if constexpr (std::is_copy_constructible_v<F>) {
				channels.push<change_request<S>>({
					.apply = [new_value = local_value, apply](data_t& d) {
						apply(d, new_value);
					},
				});
			}
		}
		else {
			if constexpr (std::is_copy_constructible_v<F>) {
				pend.apply_fns[field_key] = [new_value = local_value, apply](data_t& d) {
					apply(d, new_value);
				};
			}
			if constexpr (field_needs_restart) {
				pend.restart_fields.insert(field_key);
			}
		}
	}
	else {
		pend.apply_fns.erase(field_key);
		pend.restart_fields.erase(field_key);
	}

	if constexpr (field_needs_restart) {
		if (b.ctx.current_menu) {
			auto& ctx = b.ctx;
			const float subline_size = ctx.style.font_size * 0.85f;
			const float subline_height = ctx.font->line_height(subline_size) + ctx.style.padding * 0.25f;
			const gui::ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
			const gui::ui_rect subline_rect = gui::ui_rect::from_position_size(
				{ content_rect.left() + ctx.style.padding, ctx.layout_cursor.y() },
				{ content_rect.width() - ctx.style.padding, subline_height }
			);
			const vec4f badge_color = is_modified ? ctx.style.color_accent : ctx.style.color_text_secondary;
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
	}

	if constexpr (!describe_text.empty()) {
		if (b.ctx.current_menu) {
			const float widget_height = b.ctx.font->line_height(b.ctx.style.font_size) + b.ctx.style.padding * 0.5f;
			const gui::ui_rect content_rect = b.ctx.current_menu->rect.inset({ b.ctx.style.padding, b.ctx.style.padding });
			const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
				{ content_rect.left(), row_y_before },
				{ content_rect.width(), widget_height }
			);
			if (row_rect.contains(b.ctx.input.mouse_position()) && b.ctx.input_available()) {
				const gse::id tooltip_id = gui::ids::make_from_key(hash_combine(category_hash, tooltip_suffix_hash));
				b.ctx.set_tooltip(tooltip_id, describe_text);
			}
		}
	}
}

template <gse::has_settings S, bool HotOnly>
auto gse::settings::draw_fields(gui::builder& b, panel_state& ps, const typename S::data& live, channel_writer& channels) -> void {
	using data_t = typename S::data;

	auto& pend = ps.template pending_for<S>(live);

	constexpr std::string_view category = category_of<data_t>();
	const std::uint64_t category_hash = stable_id(category);

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^data_t, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{} && (!HotOnly || has_annotation<settings::hot_reloadable_tag>(m))) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view label = meta::member_name(m);
			constexpr std::uint64_t label_hash = stable_id(label);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				const std::uint64_t nested_hash = hash_combine(category_hash, label_hash);
				template for (constexpr auto sub : std::define_static_array(std::meta::nonstatic_data_members_of(^^F, std::meta::access_context::unchecked()))) {
					if constexpr (meta::find_describe(sub) != std::meta::info{} && (!HotOnly || has_annotation<settings::hot_reloadable_tag>(sub))) {
						using sub_t = [:std::meta::type_of(sub):];
						static const std::string sub_label = pretty_label(meta::member_name(sub));
						const std::uint64_t sub_key = hash_combine(nested_hash, stable_id(meta::member_name(sub)));

						if constexpr (has_annotation<settings::hot_reloadable_tag>(sub)) {
							pend.value.[:m:].[:sub:] = live.[:m:].[:sub:];
						}

						draw_one<S, sub, sub_t>(b, ps, pend, channels, category_hash, sub_key, sub_label, pend.value.[:m:].[:sub:], live.[:m:].[:sub:], [](data_t& d, const sub_t& v) {
							d.[:m:].[:sub:] = v;
						});
					}
				}
			}
			else {
				static const std::string display_label = pretty_label(label);
				const std::uint64_t field_key = hash_combine(category_hash, label_hash);

				if constexpr (has_annotation<settings::hot_reloadable_tag>(m)) {
					pend.value.[:m:] = live.[:m:];
				}

				draw_one<S, m, F>(b, ps, pend, channels, category_hash, field_key, display_label, pend.value.[:m:], live.[:m:], [](data_t& d, const F& v) {
					d.[:m:] = v;
				});
			}
		}
	}
}

auto gse::settings::panel_state::has_pending() const -> bool {
	return pending_count() > 0;
}

auto gse::settings::panel_state::pending_count() const -> std::size_t {
	std::size_t total = 0;
	for (const auto& entry : std::views::values(pending_by_type)) {
		total += entry->modified_count();
	}
	return total;
}

auto gse::settings::panel_state::pending_restart_count() const -> std::size_t {
	std::size_t total = 0;
	for (const auto& entry : std::views::values(pending_by_type)) {
		total += entry->restart_count();
	}
	return total;
}

auto gse::settings::panel_state::needs_restart() const -> bool {
	return restart_pending_applied;
}

auto gse::settings::panel_state::apply_all(channel_writer& channels) -> void {
	for (auto& entry : std::views::values(pending_by_type)) {
		if (entry->modified_count() == 0) {
			continue;
		}
		if (entry->restart_count() > 0) {
			restart_pending_applied = true;
		}
		entry->push(channels);
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
		if (entry.category.empty()) {
			return;
		}
		if (seen.insert(entry.category).second) {
			category_order.push_back(entry.category);
		}
	});
	std::ranges::sort(category_order);

	for (const auto& cat : category_order) {
		const register_settings_type* page_entry = nullptr;
		save_reg.for_each_entry([&](const register_settings_type& entry) {
			if (page_entry || entry.category != cat) {
				return;
			}
			if (entry.draw_page && entry.settings_ptr) {
				page_entry = &entry;
			}
		});

		if (page_entry) {
			page_entry->draw_page(&b, &ps, page_entry->settings_ptr, &channels);
			continue;
		}

		bool category_has_hot = false;
		save_reg.for_each_entry([&](const register_settings_type& entry) {
			if (entry.category == cat && entry.has_hot_fields) {
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
					if (entry.draw && entry.settings_ptr) {
						entry.draw(&sub, &ps, entry.category, entry.settings_ptr, &channels);
					}
				});
			}
		);
	}
}
