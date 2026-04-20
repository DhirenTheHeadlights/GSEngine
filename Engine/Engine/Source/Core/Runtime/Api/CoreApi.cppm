export module gse.runtime:core_api;

import std;

import gse.utility;
import gse.graphics;
import gse.audio;
import gse.platform;
import gse.physics;
import gse.network;
import gse.log;

import :engine;
import :world;

export namespace gse {
    struct engine_config {
        std::string title = "GSEngine Application";
        std::optional<vec2f> size = std::nullopt;
        bool resizable = true;
        bool fullscreen = false;
    };

    template <typename State>
    auto state_of() -> const State&;

    template <typename State>
    auto try_state_of() -> const State*;

    template <typename State>
    auto has_state() -> bool;

    template <typename Resources>
    auto resources_of(
    ) -> const Resources&;

    template <typename T>
    auto channel_add(
        T&& request
    ) -> void;

    template <promiseable T>
    auto channel_add(
        T&& request
    ) -> channel_future<typename std::decay_t<T>::result_type>;

    template <typename F>
    auto defer(
        F&& fn
    ) -> void;

    template <typename S, typename State, typename... Args>
    auto add_system(
        Args&&... args
    ) -> State&;
}

namespace gse {
	std::unique_ptr<engine> engine_instance = nullptr;
    std::atomic should_shutdown = false;
}

export namespace gse {
    template <typename... Hooks>
    auto start(
        flags<engine_flag> engine_flags = flags<engine_flag>{ engine_flag::create_window } | engine_flag::render,
        const engine_config& config = {}
    ) -> void;

    auto shutdown() -> void;
}

template <typename State>
auto gse::state_of() -> const State& {
    return engine_instance->state_of<State>();
}

template <typename State>
auto gse::try_state_of() -> const State* {
	return engine_instance->try_state_of<State>();
}

template <typename State>
auto gse::has_state() -> bool {
    return engine_instance && engine_instance->has_state<State>();
}

template <typename Resources>
auto gse::resources_of() -> const Resources& {
	return engine_instance->resources_of<Resources>();
}

template <typename T>
auto gse::channel_add(T&& request) -> void {
    engine_instance->channel<std::decay_t<T>>().push(std::forward<T>(request));
}

template <gse::promiseable T>
auto gse::channel_add(T&& request) -> channel_future<typename std::decay_t<T>::result_type> {
	using R = typename std::decay_t<T>::result_type;
	auto [future, promise] = make_promise<R>();
	request.promise = std::move(promise);
	engine_instance->channel<std::decay_t<T>>().push(std::forward<T>(request));
	return future;
}

template <typename F>
auto gse::defer(F&& fn) -> void {
    engine_instance->defer<first_arg_t<F>>(std::forward<F>(fn));
}

template <typename S, typename State, typename... Args>
auto gse::add_system(Args&&... args) -> State& {
    return engine_instance->add_system<S, State>(std::forward<Args>(args)...);
}

template <typename... Hooks>
auto gse::start(const flags<engine_flag> engine_flags, const engine_config& config) -> void {
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
    trace::finalize_frame();

    task::start([&] {
        const auto loop_id = find_or_generate_id("frame::loop");
        const auto poll_id = find_or_generate_id("frame::poll_events");
        const auto sync_begin_id = find_or_generate_id("frame::sync_begin");
        const auto sync_end_id = find_or_generate_id("frame::sync_end");
        const auto finalize_id = find_or_generate_id("frame::finalize_trace");
        const auto ingest_id = find_or_generate_id("frame::ingest_profile");
        const auto update_id = find_or_generate_id("engine::update");
        const auto render_id = find_or_generate_id("engine::render");

        while (!should_shutdown.load(std::memory_order_acquire)) {
            trace::scope(loop_id, [&] {
                if (engine_flags.test(engine_flag::create_window)) {
                    trace::scope(poll_id, [&] {
                        window::poll_events();
                    });
                }

                trace::scope(sync_begin_id, [&] {
                    frame_sync::begin();
                });

                trace::scope(engine_instance->id(), [&] {
                    trace::scope(update_id, [&] {
                        engine_instance->update();
                    });

                    if (engine_flags.test(engine_flag::render)) {
                        trace::scope(render_id, [&] {
                            engine_instance->render();
                        });
                    }
                });

                trace::scope(sync_end_id, [&] {
                    frame_sync::end();
                });
            });

            trace::scope(finalize_id, [&] {
                trace::finalize_frame();
            });
            trace::scope(ingest_id, [&] {
                profile::ingest_frame();
            });
        }

        task::wait_idle();
    });
}

auto gse::shutdown() -> void {
    should_shutdown.store(true, std::memory_order_release);
}