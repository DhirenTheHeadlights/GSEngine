export module gse.graphics:draw_struct;

import std;

import gse.meta;
import gse.core;
import gse.math;

import :types;
import :builder;
import :toggle_widget;
import :slider_widget;
import :dropdown_widget;
import :settings;

export namespace gse::gui {
	template <typename T>
	auto draw_struct(
		builder& b,
		T& value,
		settings::panel_state& state,
		std::string_view key_prefix = {}
	) -> void;
}

template <typename T>
auto gse::gui::draw_struct(builder& b, T& value, settings::panel_state& state, const std::string_view key_prefix) -> void {
	const std::uint64_t prefix_hash = stable_id(key_prefix);

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view label = meta::member_name(m);
			constexpr std::uint64_t label_hash = stable_id(label);
			const std::uint64_t field_key = hash_combine(prefix_hash, label_hash);

			if constexpr (has_annotation<settings::draw_with>(m)) {
				constexpr auto dw = annotation_of<settings::draw_with, m>();
				dw.fn(b, state, label, &value.[:m:]);
			}
			else if constexpr (std::same_as<F, bool>) {
				b.draw<toggle>({
					.name = label,
					.value = value.[:m:],
				});
			}
			else if constexpr (settings::is_choice_v<F>) {
				auto& dd_state = state.dropdowns[field_key];
				std::size_t idx = settings::choice_index(value.[:m:]);
				const auto r = b.draw<dropdown>({
					.name = label,
					.current_index = idx,
					.options = value.[:m:]
					.options,
					.state = dd_state,
				});
				if (r.changed) {
					settings::set_choice_index(value.[:m:], idx);
				}
			}
			else if constexpr (std::is_enum_v<F>) {
				auto& dd_state = state.dropdowns[field_key];

				static const std::vector<std::string> options = [] {
					std::vector<std::string> v;
					template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^F))) {
						v.emplace_back(std::meta::identifier_of(e));
					}
					return v;
				}();

				static const std::vector<F> values = [] {
					std::vector<F> v;
					template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^F))) {
						v.push_back([:e:]);
					}
					return v;
				}();

				std::size_t idx = 0;
				for (std::size_t i = 0; i < values.size(); ++i) {
					if (values[i] == value.[:m:]) {
						idx = i;
						break;
					}
				}

				const auto r = b.draw<dropdown>({
					.name = label,
					.current_index = idx,
					.options = options,
					.state = dd_state,
				});

				if (r.changed && idx < values.size()) {
					value.[:m:] = values[idx];
				}
			}
			else if constexpr (constexpr auto range_t = meta::find_range(m); range_t != std::meta::info{}) {
				using R = [:range_t:];
				if constexpr (internal::is_quantity<F>) {
					b.draw<quantity_slider<F, typename F::default_unit{}>>({
						.name = label,
						.value = value.[:m:],
						.min = R::min,
						.max = R::max,
					});
				}
				else {
					b.draw<slider<F>>({
						.name = label,
						.value = value.[:m:],
						.min = static_cast<F>(R::min),
						.max = static_cast<F>(R::max),
					});
				}
			}
		}
	}
}