export module gse.runtime:core_api;

import std;

import gse.utility;
import gse.graphics;
import gse.platform;
import gse.physics;
import gse.network;
import gse.log;

import :engine;
import :world;

export namespace gse {
    struct engine_config {
        std::string title = "GSEngine Application";
        std::optional<unitless::vec2> size = std::nullopt;
        bool resizable = true;
        bool fullscreen = false;
    };

    template <typename State>
    auto state_of() -> State&;

    template <typename State>
    auto has_state() -> bool;

    template <typename T>
    auto channel_add(
        T&& request
    ) -> void;
}

namespace gse {
	std::unique_ptr<engine> engine_instance = nullptr;
    std::atomic should_shutdown = false;
}

export namespace gse {
    template <typename... Hooks>
    auto start(
        flags engine_flags = flags::create_window | flags::render,
        const engine_config& config = {}
    ) -> void;

    auto shutdown() -> void;
}

template <typename State>
auto gse::state_of() -> State& {
    return engine_instance->state_of<State>();
}

template <typename State>
auto gse::has_state() -> bool {
    return engine_instance && engine_instance->has_state<State>();
}

template <typename T>
auto gse::channel_add(T&& request) -> void {
    engine_instance->channel<std::decay_t<T>>().push(std::forward<T>(request));
}

template <typename... Hooks>
auto gse::start(const flags engine_flags, const engine_config& config) -> void {
    engine_instance = std::make_unique<engine>(config.title, engine_flags);
	log::println(log::level::info, "Starting GSEngine...");

    (engine_instance->add_hook<Hooks>(), ...);

    auto cleanup = make_scope_exit([] {
        if (engine_instance) {
            engine_instance->shutdown();
            engine_instance.reset();
        }
    });

    engine_instance->initialize();
    task::start([&] {
        while (!should_shutdown.load(std::memory_order_acquire)) {
            if (has_flag(engine_flags, flags::create_window)) {
                window::poll_events();
            }

            frame_sync::begin();

            trace::scope(engine_instance->id(), [&] {
                task::group frame_tasks;

                frame_tasks.post(
                    [&] {
	                    engine_instance->update();
	                },
                    find_or_generate_id("Engine::Update")
                );

                if (has_flag(engine_flags, flags::render)) {
                    frame_tasks.post(
                        [&] {
                            engine_instance->render();
                        },
                        find_or_generate_id("Engine::Render")
                    );
                }
            });

            frame_sync::end();
            trace::finalize_frame();
        }

        task::wait_idle();
    });
}

auto gse::shutdown() -> void {
    should_shutdown.store(true, std::memory_order_release);
}