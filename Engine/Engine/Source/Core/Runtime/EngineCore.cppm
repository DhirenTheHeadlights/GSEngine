export module gse.runtime:engine;

import std;

import :scene_loader;

import gse.utility;
import gse.graphics;
import gse.platform;
import gse.assert;
import gse.network;

export namespace gse {
	struct engine : hookable<engine> {
		explicit engine(const std::string& name) : hookable(name) {}
	};

	using world = hook<engine>;

	struct engine_config {
		std::string title = "GSEngine Application";
		std::optional<unitless::vec2> size = std::nullopt;
		bool resizable = true;
		bool fullscreen = false;
	};

	enum struct flags : std::uint8_t {
		none = 0,
		create_window = 1 << 0,
		render = 1 << 1,
	};

	constexpr auto operator|(
		flags lhs,
		flags rhs
	) -> flags;

	constexpr auto has_flag(
		flags haystack,
		flags needle
	) -> bool;
}

namespace gse {
	auto initialize(
		flags engine_flags, 
		const engine_config& config
	) -> void;

	auto update(
		flags engine_flags, 
		const engine_config& config
	) -> void;

	auto render(
		flags engine_flags, 
		const engine_config& config
	) -> void;

	engine engine("GSEngine");
	bool should_shutdown = false;

	class frame_barrier {
	public:
		auto rendezvous(
			const std::function<void()>& flip_once
		) -> void;
	private:
		std::mutex m_mutex;
		std::condition_variable m_cv;
		std::int32_t m_arrived = 0;
		bool m_flip_pending = false;
	};

	frame_barrier frame_barrier;
	std::jthread render_thread;
	std::atomic_bool running = false;
}

export namespace gse {
	template <typename... Args>
	auto start(
		flags engine_flags = flags::create_window | flags::render,
		const engine_config& config = {}
	) -> void;

	auto shutdown(
	) -> void;
}

constexpr auto gse::operator|(flags lhs, flags rhs) -> flags {
	return static_cast<flags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr auto gse::has_flag(flags haystack, flags needle) -> bool {
	return (static_cast<std::uint32_t>(haystack) & static_cast<std::uint32_t>(needle)) == static_cast<std::uint32_t>(needle);
}

auto gse::initialize(const flags engine_flags, const engine_config& config) -> void {
	network::initialize();
	scene_loader::initialize();
	engine.initialize();
}

auto gse::update(const flags engine_flags, const engine_config& config) -> void {
	add_timer("Engine::update", [] {
		window::poll_events();

		system_clock::update();
		scene_loader::update();

		engine.update();
	});
}

auto gse::render(const flags engine_flags, const engine_config& config) -> void {
	add_timer("Engine::render", [] {
		scene_loader::render([] {
			engine.render();
		});
	});
}

auto gse::frame_barrier::rendezvous(const std::function<void()>& flip_once) -> void {
	std::unique_lock lock(m_mutex);
	if (++m_arrived < 2) {
		m_cv.wait(lock, [&] { return m_arrived == 2 && !m_flip_pending; });
	}
	else {
		m_flip_pending = true;
		flip_once();
		m_arrived = 2;
		m_flip_pending = false;
		m_cv.notify_one();
		m_arrived = 0;
	}
}

template <typename... Args>
auto gse::start(const flags engine_flags, const engine_config& config) -> void {
	(engine.add_hook(std::make_unique<Args>(&engine)), ...);

	initialize(engine_flags, config);

	running.store(true, std::memory_order_release);
	render_thread = std::jthread([&] {
		render(engine_flags, config);
	});

	while (!should_shutdown) {
		input::update([&] {
			update(engine_flags, config);

			scene_loader::flip_registry_buffers();
		});
	}

	running.store(false, std::memory_order_release);
	if (render_thread.joinable()) {
		render_thread.join();
	}

	scene_loader::shutdown();
}

auto gse::shutdown() -> void {
	should_shutdown = true;
}





