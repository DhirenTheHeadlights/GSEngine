export module gse.runtime:core_api;

import std;
import gse.std_meta;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.graphics;
import gse.audio;
import gse.os;
import gse.assets;
import gse.gpu;
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

    auto set_networked(
        bool networked
    ) -> void;

    auto set_authoritative(
        bool authoritative
    ) -> void;

    auto set_local_controller_id(
        id controller_id
    ) -> void;

    auto set_input_sampler(
        input_sampler_fn fn
    ) -> void;

    auto add_scene(
        std::string_view name,
        scene::setup_fn setup = {}
    ) -> scene*;

    auto activate_scene(
        id scene_id
    ) -> void;

    auto deactivate_scene(
        id scene_id
    ) -> void;

    auto current_scene(
    ) -> scene*;

    auto direct(
    ) -> director;

    auto triggers(
    ) -> std::span<const trigger>;
}

namespace gse {
	std::unique_ptr<engine> engine_instance = nullptr;
    std::atomic should_shutdown = false;
}

export namespace gse {
    using app_setup_fn = std::function<void(engine&)>;

    auto start(
        app_setup_fn setup,
        flags<engine_flag> engine_flags = flags<engine_flag>{ engine_flag::create_window } | engine_flag::render,
        const engine_config& config = {}
    ) -> void;

    auto shutdown(
    ) -> void;
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
    engine_instance->defer<typename [: std::meta::type_of(std::meta::parameters_of(^^std::remove_cvref_t<F>::operator())[0]) :]>(std::forward<F>(fn));
}

template <typename S, typename State, typename... Args>
auto gse::add_system(Args&&... args) -> State& {
    return engine_instance->add_system<S, State>(std::forward<Args>(args)...);
}

auto gse::set_networked(const bool networked) -> void {
    engine_instance->set_networked(networked);
}

auto gse::set_authoritative(const bool authoritative) -> void {
    engine_instance->set_authoritative(authoritative);
}

auto gse::set_local_controller_id(const id controller_id) -> void {
    engine_instance->set_local_controller_id(controller_id);
}

auto gse::set_input_sampler(input_sampler_fn fn) -> void {
    engine_instance->set_input_sampler(std::move(fn));
}

auto gse::add_scene(const std::string_view name, scene::setup_fn setup) -> scene* {
    return engine_instance->add_scene(name, std::move(setup));
}

auto gse::activate_scene(const id scene_id) -> void {
    engine_instance->activate_scene(scene_id);
}

auto gse::deactivate_scene(const id scene_id) -> void {
    engine_instance->deactivate_scene(scene_id);
}

auto gse::current_scene() -> scene* {
    return engine_instance->current_scene();
}

auto gse::direct() -> director {
    return engine_instance->direct();
}

auto gse::triggers() -> std::span<const trigger> {
    return engine_instance->triggers();
}

auto gse::start(app_setup_fn setup, const flags<engine_flag> engine_flags, const engine_config& config) -> void {
    engine_instance = std::make_unique<engine>(config.title, engine_flags);
	log::println(log::level::info, "Starting GSEngine...");

    auto cleanup = make_scope_exit([] {
        if (engine_instance) {
            engine_instance->shutdown();
            engine_instance.reset();
        }
    });

    engine_instance->initialize();

    if (setup) {
        setup(*engine_instance);
    }
    trace::finalize_frame();

    task::start([&] {
        const auto loop_id = trace_id<"frame::loop">();
        const auto poll_id = trace_id<"frame::poll_events">();
        const auto sync_begin_id = trace_id<"frame::sync_begin">();
        const auto sync_end_id = trace_id<"frame::sync_end">();
        const auto finalize_id = trace_id<"frame::finalize_trace">();
        const auto ingest_id = trace_id<"frame::ingest_profile">();
        const auto update_id = trace_id<"engine::update">();
        const auto render_id = trace_id<"engine::render">();

        while (!should_shutdown.load(std::memory_order_acquire)) {
            {
            	trace::scope_guard sg{loop_id};
                if (engine_flags.test(engine_flag::create_window)) {
                    {
                    	trace::scope_guard sg{poll_id};
                        window::poll_events();
                    }
                }

                {
                	trace::scope_guard sg{sync_begin_id};
                    frame_sync::begin();
                }

                {
                	trace::scope_guard sg{engine_instance->id()};
                    {
                    	trace::scope_guard sg{update_id};
                        engine_instance->update();
                    }

                    if (engine_flags.test(engine_flag::render)) {
                        {
                        	trace::scope_guard sg{render_id};
                            engine_instance->render();
                        }
                    }
                }

                {
                	trace::scope_guard sg{sync_end_id};
                    frame_sync::end();
                }
            }

            {
            	trace::scope_guard sg{finalize_id};
                trace::finalize_frame();
            }
            {
            	trace::scope_guard sg{ingest_id};
                profile::ingest_frame();
            }
        }

        task::wait_idle();
    });
}

auto gse::shutdown() -> void {
    should_shutdown.store(true, std::memory_order_release);
}