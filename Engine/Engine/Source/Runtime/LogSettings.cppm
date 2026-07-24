export module gse.runtime:log_settings;

import std;

import gse.log;
import gse.ecs;
import gse.meta;
import gse.concurrency;

export namespace gse::log_settings {
	struct [[= system_state<"LogSettings">{}, = settings::category<"Dev">{}]] data {
		[[
			= settings::describe<"Minimum level applied to every log category. Individual category settings can diverge afterward.">{},
			= settings::hot_reloadable
		]]
		log::level global_level = log::level::info;

		[[
			= settings::describe<"Minimum level for general engine messages.">{},
			= settings::hot_reloadable
		]]
		log::level general_level = log::level::info;

		[[
			= settings::describe<"Minimum level for runtime and boot messages.">{},
			= settings::hot_reloadable
		]]
		log::level runtime_level = log::level::info;

		[[
			= settings::describe<"Minimum level for renderer and frame scheduling messages.">{},
			= settings::hot_reloadable
		]]
		log::level render_level = log::level::info;

		[[
			= settings::describe<"Minimum level for network messages.">{},
			= settings::hot_reloadable
		]]
		log::level network_level = log::level::info;

		[[
			= settings::describe<"Minimum level for Vulkan messages.">{},
			= settings::hot_reloadable
		]]
		log::level vulkan_level = log::level::info;

		[[
			= settings::describe<"Minimum level for Vulkan validation messages.">{},
			= settings::hot_reloadable
		]]
		log::level vulkan_validation_level = log::level::info;

		[[
			= settings::describe<"Minimum level for Vulkan memory messages.">{},
			= settings::hot_reloadable
		]]
		log::level vulkan_memory_level = log::level::info;

		[[
			= settings::describe<"Minimum level for Direct3D 12 messages.">{},
			= settings::hot_reloadable
		]]
		log::level dx12_level = log::level::info;

		[[
			= settings::describe<"Minimum level for Direct3D 12 validation messages.">{},
			= settings::hot_reloadable
		]]
		log::level dx12_validation_level = log::level::info;

		[[
			= settings::describe<"Minimum level for asset pipeline messages.">{},
			= settings::hot_reloadable
		]]
		log::level assets_level = log::level::info;

		[[
			= settings::describe<"Minimum level for task scheduler messages.">{},
			= settings::hot_reloadable
		]]
		log::level task_level = log::level::info;

		[[
			= settings::describe<"Minimum level for save-system messages.">{},
			= settings::hot_reloadable
		]]
		log::level save_system_level = log::level::info;

		[[
			= settings::describe<"Minimum level for physics messages.">{},
			= settings::hot_reloadable
		]]
		log::level physics_level = log::level::info;

		log::level m_last_global_level = log::level::debug;
	};

	[[= system_run<>{}]]
	auto run(
		data& d
	) -> async::task<>;
}

namespace gse::log_settings {
	consteval auto category_field(
		std::string_view member,
		std::string_view category
	) -> bool;

	auto set_category_fields(
		data& d,
		log::level level
	) -> void;

	auto apply_categories(
		data& d
	) -> void;
}

consteval auto gse::log_settings::category_field(const std::string_view member, const std::string_view category) -> bool {
	constexpr std::string_view suffix = "_level";
	return member.size() == category.size() + suffix.size()
		&& member.starts_with(category)
		&& member.ends_with(suffix);
}

auto gse::log_settings::set_category_fields(data& d, const log::level level) -> void {
	template for (constexpr auto category : std::define_static_array(std::meta::enumerators_of(^^log::category))) {
		template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^data, std::meta::access_context::unchecked()))) {
			if constexpr (category_field(meta::member_name(member), std::meta::identifier_of(category))) {
				d.[:member:] = level;
			}
		}
	}
}

auto gse::log_settings::apply_categories(data& d) -> void {
	template for (constexpr auto category : std::define_static_array(std::meta::enumerators_of(^^log::category))) {
		template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^data, std::meta::access_context::unchecked()))) {
			if constexpr (category_field(meta::member_name(member), std::meta::identifier_of(category))) {
				log::set_level([:category:], d.[:member:]);
			}
		}
	}
}

auto gse::log_settings::run(data& d) -> async::task<> {
	if (d.global_level != d.m_last_global_level) {
		log::set_level(d.global_level);
		set_category_fields(d, d.global_level);
		d.m_last_global_level = d.global_level;
	}

	apply_categories(d);

	return {};
}