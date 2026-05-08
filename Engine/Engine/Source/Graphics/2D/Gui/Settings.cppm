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
import :builder;
import :toggle_widget;
import :slider_widget;
import :dropdown_widget;
import :section_widget;

export namespace gse::settings {
    struct panel_state {
        std::unordered_map<std::string, gui::dropdown_state> dropdowns;
    };

    using custom_draw_fn = void(*)(
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

    template <typename T>
    auto draw_struct_thunk(
        void* gui_builder,
        void* panel_state,
        std::string_view category,
        void* settings_ptr,
        void* channels_writer
    ) -> void;
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

template <typename T>
auto gse::settings::draw_struct_thunk(void* gui_builder, void* panel_state_ptr, const std::string_view category, void* settings_ptr, void* channels_writer) -> void {
    auto& b = *static_cast<gui::builder*>(gui_builder);
    auto& ps = *static_cast<panel_state*>(panel_state_ptr);
    auto& channels = *static_cast<channel_writer*>(channels_writer);
    const auto& live = *static_cast<const T*>(settings_ptr);

    T local = live;

    b.draw<gui::section>({ .title = category });

    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if constexpr (meta::find_describe(m) != std::meta::info{}) {
            using F = [:std::meta::type_of(m):];
            constexpr std::string_view label = meta::member_name(m);
            auto& ref = local.[:m:];

            if constexpr (has_annotation<draw_with>(m)) {
                constexpr auto dw = annotation_of<draw_with, m>();
                dw.fn(b, ps, label, &ref);
            }
            else if constexpr (std::same_as<F, bool>) {
                b.draw<gui::toggle>({ .name = label, .value = ref });
            }
            else if constexpr (settings::is_choice_v<F>) {
                const std::string key = std::string(category) + "::" + std::string(label);
                auto& dd_state = ps.dropdowns[key];
                std::size_t idx = static_cast<std::size_t>(ref.value);
                const auto r = b.draw<gui::dropdown>({
                    .name = label,
                    .current_index = idx,
                    .options = ref.options,
                    .state = dd_state,
                });
                if (r.changed) {
                    ref.value = static_cast<typename F::value_type>(idx);
                }
            }
            else if constexpr (std::is_enum_v<F>) {
                const std::string key = std::string(category) + "::" + std::string(label);
                draw_enum_dropdown<F>(b, ps, key, label, ref);
            }
            else if constexpr (constexpr auto range_t = meta::find_range(m); range_t != std::meta::info{}) {
                using R = [: range_t :];
                if constexpr (gse::internal::is_quantity<F>) {
                    b.draw<gui::quantity_slider<F, typename F::default_unit{}>>({
                        .name = label,
                        .value = ref,
                        .min = R::min,
                        .max = R::max,
                    });
                }
                else {
                    b.draw<gui::slider<F>>({
                        .name = label,
                        .value = ref,
                        .min = static_cast<F>(R::min),
                        .max = static_cast<F>(R::max),
                    });
                }
            }
        }
    }

    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if constexpr (meta::find_describe(m) != std::meta::info{}) {
            using F = [:std::meta::type_of(m):];
            if constexpr (settings::is_choice_v<F>) {
                if (local.[:m:].value != live.[:m:].value) {
                    channels.push(settings::change_request<T>{
                        .apply = [new_value = local.[:m:].value](T& cfg) {
                            cfg.[:m:].value = new_value;
                        }
                    });
                }
            }
            else if constexpr (requires (F a, F b) { a == b; }) {
                if (local.[:m:] != live.[:m:]) {
                    channels.push(settings::change_request<T>{
                        .apply = [new_value = local.[:m:]](T& cfg) {
                            cfg.[:m:] = new_value;
                        }
                    });
                }
            }
        }
    }
}

auto gse::settings::panel(gui::builder& b, panel_state& ps, channel_writer& channels, const save::registry& save_reg, const std::string_view category_filter) -> void {
    for (const auto& entry : save_reg.entries()) {
        if (!category_filter.empty() && entry.category != category_filter) {
            continue;
        }
        if (entry.draw && entry.settings_ptr) {
            entry.draw(&b, &ps, entry.category, entry.settings_ptr, &channels);
        }
    }
}
