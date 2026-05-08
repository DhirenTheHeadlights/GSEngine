export module gse.runtime:engine;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.network;
import gse.graphics;
import gse.audio;
import gse.physics;
import gse.os;
import gse.gpu;
import gse.log;
import gse.save;
import gse.config;

import :scene;
import :world;

export namespace gse {
	enum class engine_flag : std::uint8_t {
		create_window = 1 << 0,
		render = 1 << 1,
	};

	struct set_networked_request {
		bool value = false;
	};

	struct set_authoritative_request {
		bool value = true;
	};

	struct set_local_controller_id_request {
		id controller_id;
	};

	struct activate_scene_request {
		id scene_id;
	};

	struct deactivate_active_scene_request {};

	class engine : public identifiable {
	public:
		using setup_fn = std::function<void(engine&)>;

		engine(
			const std::string& name,
			flags<engine_flag> engine_flags
		);

		auto initialize(
			const setup_fn& app_setup = {}
		) -> void;

		auto update(
		) -> void;

		auto render(
		) -> void;

		auto shutdown(
		) -> void;

		auto make_channel_writer(
		) -> channel_writer;

		auto world(
		) -> gse::world&;

		auto add_scene(
			std::string_view name,
			scene::setup_fn setup = {}
		) -> scene*;

		auto direct(
		) -> director;

		auto triggers(
		) const -> std::span<const trigger>;

		template <typename S, typename... Args>
		auto add_system(
			Args&&... args
		) -> state_of_t<S>&;
	private:
		auto drain_lifecycle_channels(
		) -> void;

		flags<engine_flag> m_flags;
		scheduler m_scheduler;
		save::registry m_save;
		gse::world m_world;
	};
}

namespace gse {
	template <>
	struct same_frame_channel_t<set_networked_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<set_authoritative_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<set_local_controller_id_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<activate_scene_request> : std::true_type {};

	template <>
	struct same_frame_channel_t<deactivate_active_scene_request> : std::true_type {};
}

auto gse::engine::add_scene(std::string_view name, scene::setup_fn setup) -> scene* {
	return m_world.add(name, std::move(setup));
}

namespace gse {
	template <typename T>
	auto make_settings_record(T& obj) -> settings::register_settings_type {
		return {
			.category = std::string(settings::category_of<T>()),
			.type_id = id_of<T>(),
			.settings_ptr = &obj,
			.write = &settings::write_settings_for<T>,
			.read  = &settings::read_settings_for<T>,
			.draw  = &settings::draw_struct_thunk<T>,
		};
	}
}

template <typename S, typename... Args>
auto gse::engine::add_system(Args&&... args) -> state_of_t<S>& {
	auto& state_ref = m_scheduler.add_system<S>(std::forward<Args>(args)...);
	if constexpr (has_settings<S>) {
		using settings_t = typename S::settings;
		m_save.add(make_settings_record(m_scheduler.state<settings_t>()));
	}
	return state_ref;
}
