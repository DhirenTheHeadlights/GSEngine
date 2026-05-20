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
import :world_system;

export namespace gse {
	enum class engine_flag : std::uint8_t {
		create_window = 1 << 0,
		render = 1 << 1,
	};

	class engine : public identifiable {
	public:
		using setup_fn = std::function<void(engine&)>;

		engine(const std::string& name, flags<engine_flag> engine_flags);

		auto initialize(const setup_fn& app_setup = {}) -> void;

		auto update() -> void;

		auto render() -> void;

		auto shutdown() -> void;

		auto make_channel_writer() -> channel_writer;

		auto registry() -> gse::registry&;

		auto world() -> world_system::data&;

		template <typename S, typename... Args>
		auto add_system(Args&&... args) -> system_handle<S>;

	private:
		flags<engine_flag> m_flags;
		scheduler m_scheduler;
		save::registry m_save;
		primitives::data m_primitives;
		gse::registry m_registry;
		loading::state m_loading;
	};
}

namespace gse {
	template <typename S>
	auto make_settings_record(typename S::data& obj) -> settings::register_settings_type {
		auto record = settings::build_settings_record<S>(obj);
		record.draw = &settings::draw_struct_thunk<S>;
		return record;
	}
}

template <typename S, typename... Args>
auto gse::engine::add_system(Args&&... args) -> system_handle<S> {
	auto handle = m_scheduler.add_system<S>(std::forward<Args>(args)...);
	if constexpr (has_settings<S>) {
		using data_t = typename S::data;
		m_save.add(make_settings_record<S>(handle.state()));
	}
	return handle;
}
