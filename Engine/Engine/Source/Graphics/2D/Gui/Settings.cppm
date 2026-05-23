export module gse.graphics:settings;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
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
import :toggle_widget;
import :slider_widget;
import :dropdown_widget;
import :section_widget;
import :value_widget;
import :text_input_widget;

export namespace gse::settings {
	struct dimensioned_input_state {
		gui::text_input_state input_state;
		gui::dropdown_state dropdown_state;
		std::size_t selected_unit_index = 0;
		std::vector<std::string> unit_names;
		bool initialized = false;
	};

	struct panel_state {
		std::unordered_map<std::string, gui::dropdown_state> dropdowns;
		std::unordered_map<std::string, std::string> input_buffers;
		std::unordered_map<std::string, gui::text_input_state> input_states;
		std::unordered_map<std::string, dimensioned_input_state> dimensioned_states;
	};

	template <typename Q>
	struct quantity_unit_op {
		using value_type = typename Q::value_type;
		std::string name;
		value_type (*to_value)(const Q&);
		Q (*from_value)(value_type);
	};

	template <typename Q, typename U>
	auto make_unit_op() -> quantity_unit_op<Q> {
		constexpr U unit_inst{};
		return {
			.name = std::string(std::string_view(unit_inst.unit_name)),
			.to_value = +[](const Q& v) -> typename Q::value_type {
				return gse::internal::value_in<U>(v);
			},
			.from_value = +[](typename Q::value_type val) -> Q {
				return Q::template from<U>(val);
			},
		};
	}

	template <typename Q>
	using unit_family_tag_t = typename Q::default_unit::quantity_tag;

	template <typename Q>
	auto unit_ops_for() -> const std::vector<quantity_unit_op<Q>>& {
		static const std::vector<quantity_unit_op<Q>> ops = [] {
			std::vector<quantity_unit_op<Q>> result;
			constexpr auto units_tuple = gse::internal::quantity_units<unit_family_tag_t<Q>>::units;
			std::apply([&result](const auto&... us) {
				(result.push_back(make_unit_op<Q, std::remove_cvref_t<decltype(us)>>()), ...);
			}, units_tuple);
			return result;
		}();
		return ops;
	}

	template <typename Q>
	auto default_unit_index_for() -> std::size_t {
		using DefaultU = typename Q::default_unit;
		constexpr auto units_tuple = gse::internal::quantity_units<unit_family_tag_t<Q>>::units;
		std::size_t result = 0;
		std::size_t cursor = 0;
		std::apply([&](const auto&... us) {
			auto visit = [&](auto unit_inst) {
				using U = std::remove_cvref_t<decltype(unit_inst)>;
				if (std::is_same_v<DefaultU, U>) {
					result = cursor;
				}
				++cursor;
			};
			(visit(us), ...);
		}, units_tuple);
		return result;
	}

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

	auto panel(
		gui::builder& b,
		panel_state& ps,
		channel_writer& channels,
		const save::registry& save_reg,
		std::string_view category_filter = ""
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
	struct gui_draw_provider<S> {
		static constexpr draw_settings_thunk value = &draw_struct_thunk<S>;
	};
}

namespace gse::settings {
	template <typename E>
	auto draw_enum_dropdown(
		gui::builder& b,
		panel_state& ps,
		std::string_view key,
		std::string_view label,
		E& ref
	) -> void;
}

template <typename E>
auto gse::settings::draw_enum_dropdown(gui::builder& b, panel_state& ps, const std::string_view key, const std::string_view label, E& ref) -> void {
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

	auto& dd_state = ps.dropdowns[std::string(key)];

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
	using data_t = typename S::data;
	auto& b = *static_cast<gui::builder*>(gui_builder);
	auto& ps = *static_cast<panel_state*>(panel_state_ptr);
	auto& channels = *static_cast<channel_writer*>(channels_writer);
	const auto& live = *static_cast<const data_t*>(settings_ptr);

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^data_t, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view label = meta::member_name(m);
			using describe_t = [:meta::find_describe(m):];
			constexpr std::string_view describe_text = describe_t::value;
			F local_value = live.[:m:];

			const float row_y_before = b.ctx.current_menu ? b.ctx.layout_cursor.y() : 0.f;

			if constexpr (has_annotation<draw_with>(m)) {
				constexpr auto dw = annotation_of<draw_with, m>();
				dw.fn(b, ps, label, &local_value);
			}
			else if constexpr (std::same_as<F, bool>) {
				b.draw<gui::toggle>({
					.name = label,
					.value = local_value
				});
			}
			else if constexpr (settings::is_choice_v<F>) {
				const std::string key = std::string(category) + "::" + std::string(label);
				auto& dd_state = ps.dropdowns[key];
				std::size_t idx = static_cast<std::size_t>(local_value.value);
				const auto r = b.draw<gui::dropdown>({
					.name = label,
					.current_index = idx,
					.options = local_value.options,
					.state = dd_state,
				});
				if (r.changed) {
					local_value.value = static_cast<typename F::value_type>(idx);
				}
			}
			else if constexpr (std::is_enum_v<F>) {
				const std::string key = std::string(category) + "::" + std::string(label);
				draw_enum_dropdown<F>(b, ps, key, label, local_value);
			}
			else if constexpr (constexpr auto range_t = meta::find_range(m); range_t != std::meta::info{}) {
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
						.name = label,
						.value = local_value,
						.min = min_q,
						.max = max_q,
					});
				}
				else {
					b.draw<gui::slider<F>>({
						.name = label,
						.value = local_value,
						.min = static_cast<F>(R::min),
						.max = static_cast<F>(R::max),
					});
				}
			}
			else if constexpr (gse::internal::is_quantity<F>) {
				const std::string key = std::string(category) + "::" + std::string(label);
				auto& dim_state = ps.dimensioned_states[key];
				const auto& ops = unit_ops_for<F>();
				auto& buffer = ps.input_buffers[key];

				if (!dim_state.initialized) {
					dim_state.selected_unit_index = default_unit_index_for<F>();
					dim_state.unit_names.reserve(ops.size());
					for (const auto& op : ops) {
						dim_state.unit_names.push_back(op.name);
					}
					buffer = std::format("{}", ops[dim_state.selected_unit_index].to_value(live.[:m:]));
					dim_state.initialized = true;
				}

				if (dim_state.selected_unit_index >= ops.size()) {
					dim_state.selected_unit_index = 0;
				}
				const auto& current_op = ops[dim_state.selected_unit_index];
				const bool has_unit_picker = ops.size() > 1;

				if (has_unit_picker) {
					auto& ctx = b.ctx;
					if (ctx.current_menu) {
						const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
						const gse::gui::ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
						const float row_y = ctx.layout_cursor.y();
						const float label_width = content_rect.width() * 0.40f;
						const float unit_width = content_rect.width() * 0.15f;
						const float value_width = content_rect.width() - label_width - unit_width;

						const gse::gui::ui_rect label_rect = gse::gui::ui_rect::from_position_size(
							{ content_rect.left(), row_y },
							{ label_width, widget_height }
						);
						const gse::gui::ui_rect value_rect = gse::gui::ui_rect::from_position_size(
							{ content_rect.left() + label_width, row_y },
							{ value_width, widget_height }
						);
						const gse::gui::ui_rect unit_rect = gse::gui::ui_rect::from_position_size(
							{ content_rect.left() + label_width + value_width, row_y },
							{ unit_width, widget_height }
						);

						ctx.queue_text({
							.font = ctx.font,
							.text = std::string(label),
							.position = { label_rect.left(), label_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
							.scale = ctx.style.font_size,
							.color = ctx.style.color_text,
							.clip_rect = label_rect,
						});

						const gse::id input_id = gse::gui::ids::make(key + "##Input");
						gse::gui::draw::text_input_in_rect(ctx, input_id, buffer, dim_state.input_state, value_rect, b.hot_widget_id, b.focus_widget_id);

						const std::size_t prev_index = dim_state.selected_unit_index;
						const auto dd_result = gse::gui::draw::dropdown_in_rect(
							ctx,
							key + "##Unit",
							dim_state.selected_unit_index,
							dim_state.unit_names,
							dim_state.dropdown_state,
							unit_rect,
							b.hot_widget_id,
							b.active_widget_id
						);
						if (dd_result.changed) {
							dim_state.selected_unit_index = dd_result.new_index;
						}
						if (dim_state.selected_unit_index != prev_index) {
							const auto& new_op = ops[dim_state.selected_unit_index];
							buffer = std::format("{}", new_op.to_value(live.[:m:]));
						}

						ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
					}
				}
				else {
					b.draw<gui::text_input>({
						.name = label,
						.buffer = buffer,
						.state = dim_state.input_state,
					});
				}

				typename F::value_type typed_value{};
				if (gse::parse(buffer, typed_value)) {
					const F parsed_q = current_op.from_value(typed_value);
					if (parsed_q != live.[:m:]) {
						channels.push<settings::change_request<S>>({
							.apply = [new_value = parsed_q](data_t& d) {
								d.[:m:] = new_value;
							}
						});
					}
				}
			}
			else if constexpr (gse::is_arithmetic<F>) {
				const std::string key = std::string(category) + "::" + std::string(label);
				auto it = ps.input_buffers.find(key);
				if (it == ps.input_buffers.end()) {
					std::string init;
					std::format_to(std::back_inserter(init), "{}", live.[:m:]);
					it = ps.input_buffers.emplace(std::move(key), std::move(init)).first;
				}
				auto& buffer = it->second;

				auto& state = ps.input_states[key];

				b.draw<gui::text_input>({
					.name = label,
					.buffer = buffer,
					.state = state,
				});

				F parsed{};
				if (gse::parse(buffer, parsed) && parsed != live.[:m:]) {
					channels.push<settings::change_request<S>>({
						.apply = [new_value = parsed](data_t& d) {
							d.[:m:] = new_value;
						}
					});
				}
			}
			else if constexpr (gse::is_vec<F>) {
				using elem_t = typename F::value_type;
				if constexpr (gse::is_arithmetic<elem_t>) {
					b.draw<gui::vec_value<elem_t, F::extent>>({
						.name = label,
						.val = local_value,
					});
				}
				else if constexpr (gse::internal::is_quantity<elem_t>) {
					b.draw<gui::quantity_vec_value<elem_t, F::extent>>({
						.name = label,
						.val = local_value,
					});
				}
			}

			if constexpr (settings::is_choice_v<F>) {
				if (local_value.value != live.[:m:].value) {
					channels.push<settings::change_request<S>>({
						.apply = [new_value = local_value.value](data_t& d) {
							d.[:m:].value = new_value;
						}
					});
				}
			}
			else if constexpr (requires(F a, F b_) { a == b_; }) {
				if (local_value != live.[:m:]) {
					channels.push<settings::change_request<S>>({
						.apply = [new_value = local_value](data_t& d) {
							d.[:m:] = new_value;
						}
					});
				}
			}

			if (b.ctx.current_menu && !describe_text.empty()) {
				const float widget_height = b.ctx.font->line_height(b.ctx.style.font_size) + b.ctx.style.padding * 0.5f;
				const gui::ui_rect content_rect = b.ctx.current_menu->rect.inset({ b.ctx.style.padding, b.ctx.style.padding });
				const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
					{ content_rect.left(), row_y_before },
					{ content_rect.width(), widget_height }
				);
				if (row_rect.contains(b.ctx.input.mouse_position()) && b.ctx.input_available()) {
					b.ctx.set_tooltip(
						gui::ids::make(std::string(category) + "::" + std::string(label) + "##tooltip"),
						std::string(describe_text)
					);
				}
			}
		}
	}
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
		b.draw<gui::section>({
			.title = cat,
		});

		save_reg.for_each_entry([&](const register_settings_type& entry) {
			if (entry.category != cat) {
				return;
			}
			if (entry.draw && entry.settings_ptr) {
				entry.draw(&b, &ps, entry.category, entry.settings_ptr, &channels);
			}
		});
	}
}
