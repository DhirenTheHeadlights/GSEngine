import std;

import gse;
import gse.config;
import gse.scenario;
import gse.system_manifest;
import sandbox;

namespace sandbox::startup {
	struct config {
		gse::engine_config engine;
		bool use_gpu_solver = false;
		bool physics_parity = false;
		std::size_t physics_parity_envs = 1;
		std::string compare_states_a;
		std::string compare_states_b;
		double compare_states_threshold = 0.01;
		std::string scan_states;
		double scan_states_speed = 20.0;
		int compare_states_focus_frame = -1;
		int compare_states_focus_bodies = 5;
		int compare_states_focus_window = 4;
	};

	auto run_game(
		const gse::engine_config& engine
	) -> void;

	auto run_physics_parity(
		const config& cfg
	) -> void;

	auto apply_scenario(
		config& cfg
	) -> bool;

	using frame_index = std::map<std::uint32_t, std::map<std::uint64_t, gse::state_dump_record>>;

	auto trace_focus_frame(
		const config& cfg,
		const frame_index& frames_a,
		const frame_index& frames_b
	) -> void;

	auto run_compare_states(
		const config& cfg
	) -> int;

	auto run_scan_states(
		const config& cfg
	) -> int;

	// gse::network::run is a template over the game's replicated component set, so the manifest
	// needs its reflection handle rather than the template itself.
	template <typename... Components>
	consteval auto network_run_hook(gse::type_pack<Components...>) -> std::meta::info {
		return ^^gse::network::run<Components...>;
	}
}

auto sandbox::startup::run_game(const gse::engine_config& engine) -> void {
	gse::engine_config resolved = engine;
	if (resolved.project_settings_path.empty()) {
		resolved.project_settings_path = gse::config::project_settings_path();
	}

	gse::start(
		[render = resolved.render](gse::engine& e) -> void {
			constexpr auto network_run = network_run_hook(networked_components{});
			static_assert(gse::meta::find_system_hook_anno(network_run) != std::meta::info{}, "network run hook annotation not visible on the templated instantiation");
			gse::system_manifest<^^gse::network::data, network_run, ^^gse::network::shutdown>{}.register_with(e);
			gse::register_systems<^^sandbox::client_system>(e);
			gse::system_manifest<^^dev_spawn::data, ^^dev_spawn::run>{}.register_with(e);
			if (render) {
				gse::system_manifest<^^crosshair::data, ^^crosshair::draw_settings>{}.register_with(e);
				gse::system_manifest<^^client_ui::data, ^^client_ui::run, ^^pause_menu::data, ^^pause_menu::run>{}.register_with(e);
				gse::register_systems<^^gse::gui::popout_system>(e);
			}
			world_loader_setup(e);
		},
		resolved
	);
}

auto sandbox::startup::run_physics_parity(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	gse::start(
		[n_envs = cfg.physics_parity_envs](gse::engine& e) -> void {
			gse::register_systems<^^sandbox::physics_parity>(e);
			auto* scene = physics_parity_world_setup(e, n_envs);
			if (scene) {
				gse::activate_scene(e.world(), scene->id());
			}
		},
		{
			.title = "Sandbox Physics Parity",
			.create_window = false,
			.render = false,
			.use_gpu_solver = cfg.use_gpu_solver,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto sandbox::startup::trace_focus_frame(const config& cfg, const frame_index& frames_a, const frame_index& frames_b) -> void {
	const auto focus = static_cast<std::uint32_t>(cfg.compare_states_focus_frame);
	const auto at_a = frames_a.find(focus);
	const auto at_b = frames_b.find(focus);
	if (at_a == frames_a.end() || at_b == frames_b.end()) {
		std::cout << std::format("\nfocus frame {} is not present in both dumps\n", focus);
		return;
	}

	std::vector<std::pair<gse::length, std::uint64_t>> ranked;
	for (const auto& [owner, record_a] : at_a->second) {
		const auto other = at_b->second.find(owner);
		if (other != at_b->second.end()) {
			ranked.emplace_back(gse::distance(record_a.position, other->second.position), owner);
		}
	}
	std::ranges::sort(ranked, std::ranges::greater{}, &std::pair<gse::length, std::uint64_t>::first);

	const auto shown = std::min<std::size_t>(ranked.size(), static_cast<std::size_t>(std::max(cfg.compare_states_focus_bodies, 1)));
	const auto window = static_cast<std::uint32_t>(std::max(cfg.compare_states_focus_window, 0));
	const auto begin = focus > window ? focus - window : 0u;

	std::cout << std::format("\n=== focus frame {}, top {} diverging bodies ===\n", focus, shown);

	for (std::size_t i = 0; i < shown; ++i) {
		const auto owner = ranked[i].second;
		std::cout << std::format("\nbody {}\n", owner);
		std::cout << std::format("{:>7}  {:>14}  {:>13}  {:>13}  {}\n", "frame", "drift", "speed A", "speed B", "position A");

		for (std::uint32_t frame = begin; frame <= focus + window; ++frame) {
			const auto fa = frames_a.find(frame);
			const auto fb = frames_b.find(frame);
			if (fa == frames_a.end() || fb == frames_b.end()) {
				continue;
			}
			const auto ra = fa->second.find(owner);
			const auto rb = fb->second.find(owner);
			if (ra == fa->second.end() || rb == fb->second.end()) {
				continue;
			}
			std::cout << std::format(
				"{:>7}  {:>14.6f:m}  {:>13.3f}  {:>13.3f}  {:.3f}\n",
				frame,
				gse::distance(ra->second.position, rb->second.position),
				gse::magnitude(ra->second.velocity),
				gse::magnitude(rb->second.velocity),
				ra->second.position
			);
		}
	}
}

auto sandbox::startup::run_scan_states(const config& cfg) -> int {
	const auto dump = gse::read_state_dump(cfg.scan_states);
	if (!dump) {
		std::cerr << std::format("could not read {}\n", cfg.scan_states);
		return 1;
	}

	const gse::velocity limit = gse::meters_per_second(static_cast<float>(cfg.scan_states_speed));

	std::map<std::uint32_t, std::pair<gse::velocity, std::uint64_t>> worst;
	std::map<std::uint32_t, std::size_t> over;

	for (const auto& record : *dump) {
		const auto speed = gse::magnitude(record.velocity);
		auto& slot = worst[record.frame];
		if (speed > slot.first) {
			slot = { speed, record.owner };
		}
		if (speed > limit) {
			++over[record.frame];
		}
	}

	std::cout << std::format("{} records over {} frames, flagging speed > {:.1f}\n\n", dump->size(), worst.size(), limit);
	std::cout << std::format("{:>7}  {:>14}  {:>7}  {:>22}\n", "frame", "max speed", "over", "worst body");

	gse::velocity run_max{};
	std::uint32_t run_max_frame = 0;
	std::uint64_t run_max_owner = 0;
	bool first_breach = false;

	for (const auto& [frame, entry] : worst) {
		const auto [speed, owner] = entry;
		if (speed > run_max) {
			run_max = speed;
			run_max_frame = frame;
			run_max_owner = owner;
		}
		const auto over_count = over.contains(frame) ? over.at(frame) : 0;
		if (!first_breach && speed > limit) {
			first_breach = true;
			std::cout << std::format("{:>7}  {:>14.3f}  {:>7}  {:>22}  <- first over limit\n", frame, speed, over_count, owner);
		}
		else if (frame % 25 == 0) {
			std::cout << std::format("{:>7}  {:>14.3f}  {:>7}  {:>22}\n", frame, speed, over_count, owner);
		}
	}

	std::cout << std::format("\nrun max {:.3f} at frame {} on body {}\n", run_max, run_max_frame, run_max_owner);
	return 0;
}

auto sandbox::startup::run_compare_states(const config& cfg) -> int {
	const auto a = gse::read_state_dump(cfg.compare_states_a);
	const auto b = gse::read_state_dump(cfg.compare_states_b);
	if (!a || !b) {
		const auto& bad = !a ? a.error() : b.error();
		std::cerr << std::format(
			"could not read {}: magic {:#x} version {} readable {}\n",
			!a ? cfg.compare_states_a : cfg.compare_states_b,
			bad.magic,
			bad.version,
			bad.readable
		);
		return 1;
	}

	const auto by_frame = [](const std::vector<gse::state_dump_record>& records) {
		frame_index out;
		for (const auto& record : records) {
			out[record.frame][record.owner] = record;
		}
		return out;
	};

	const auto frames_a = by_frame(*a);
	const auto frames_b = by_frame(*b);

	std::cout << std::format(
		"A: {} records over {} frames\nB: {} records over {} frames\n\n",
		a->size(),
		frames_a.size(),
		b->size(),
		frames_b.size()
	);

	const gse::length threshold = gse::meters(static_cast<float>(cfg.compare_states_threshold));
	gse::length previous_max{};
	gse::length worst_jump_from{};
	gse::length worst_jump_to{};
	std::uint32_t worst_jump_frame = 0;
	std::uint64_t worst_jump_owner = 0;
	bool first_breach_reported = false;

	std::cout << std::format("{:>7}  {:>14}  {:>8}  {:>22}\n", "frame", "max drift", "over", "worst body");

	for (const auto& [frame, bodies_a] : frames_a) {
		const auto it = frames_b.find(frame);
		if (it == frames_b.end()) {
			continue;
		}

		gse::length max_delta{};
		std::uint64_t max_owner = 0;
		std::size_t over_threshold = 0;

		for (const auto& [owner, record_a] : bodies_a) {
			const auto other = it->second.find(owner);
			if (other == it->second.end()) {
				continue;
			}
			const gse::length delta = gse::distance(record_a.position, other->second.position);
			if (delta > threshold) {
				++over_threshold;
			}
			if (delta > max_delta) {
				max_delta = delta;
				max_owner = owner;
			}
		}

		if (max_delta > previous_max * 10.f && previous_max > gse::meters(1e-6f) && max_delta - previous_max > worst_jump_to - worst_jump_from) {
			worst_jump_from = previous_max;
			worst_jump_to = max_delta;
			worst_jump_frame = frame;
			worst_jump_owner = max_owner;
		}

		if (!first_breach_reported && max_delta > threshold) {
			first_breach_reported = true;
			std::cout << std::format("{:>7}  {:>14.6f:m}  {:>8}  {:>22}  <- first over threshold\n", frame, max_delta, over_threshold, max_owner);
		}
		else if (frame % 50 == 0 || std::next(frames_a.find(frame)) == frames_a.end()) {
			std::cout << std::format("{:>7}  {:>14.6f:m}  {:>8}  {:>22}\n", frame, max_delta, over_threshold, max_owner);
		}

		previous_max = max_delta;
	}

	if (cfg.compare_states_focus_frame >= 0) {
		trace_focus_frame(cfg, frames_a, frames_b);
	}

	std::cout << std::format("\nthreshold {:.6f:m}\n", threshold);
	if (worst_jump_frame != 0) {
		std::cout << std::format(
			"largest single-frame jump: frame {}, {:.6f:m} -> {:.6f:m} on body {}\n",
			worst_jump_frame,
			worst_jump_from,
			worst_jump_to,
			worst_jump_owner
		);
	}
	else {
		std::cout << "no single-frame jump above 10x; divergence is smooth drift\n";
	}

	return 0;
}

auto sandbox::startup::apply_scenario(config& cfg) -> bool {
	const auto table = gse::scenario::registry<^^sandbox::scenarios>();
	const auto* selected = gse::scenario::find(table, cfg.engine.bench.scenario);
	if (!selected) {
		std::cerr << std::format("unknown scenario '{}'; available:\n", cfg.engine.bench.scenario);
		for (const auto& entry : table) {
			std::cerr << std::format("  {}\n", std::string_view(entry.info.name));
		}
		return false;
	}

	const gse::bench_config defaults;
	auto& bench = cfg.engine.bench;

	bench.enabled = true;
	bench.scenario_body = selected->body;
	if (bench.scene == defaults.scene) {
		bench.scene = selected->info.scene;
	}
	if (bench.warmup_frames == defaults.warmup_frames) {
		bench.warmup_frames = selected->info.warmup_frames;
	}
	if (bench.frames == defaults.frames) {
		bench.frames = selected->info.frames;
	}
	if (selected->info.headless) {
		cfg.engine.create_window = false;
		cfg.engine.render = false;
	}
	if (selected->info.gpu_solver) {
		cfg.engine.use_gpu_solver = true;
	}
	cfg.engine.persist_settings = false;

	return true;
}

auto main(int argc, char** argv) -> int {
	auto cfg = gse::parse_args<sandbox::startup::config>(argc, argv);
	if (!cfg.scan_states.empty()) {
		return sandbox::startup::run_scan_states(cfg);
	}
	if (!cfg.compare_states_a.empty() || !cfg.compare_states_b.empty()) {
		if (cfg.compare_states_a.empty() || cfg.compare_states_b.empty()) {
			std::cerr << "--compare-states-a and --compare-states-b must both be given\n";
			return 1;
		}
		return sandbox::startup::run_compare_states(cfg);
	}
	if (!cfg.engine.bench.scenario.empty()) {
		if (cfg.physics_parity) {
			std::cerr << "--engine-bench-scenario and --physics-parity select different run modes; pick one\n";
			return 1;
		}
		if (!sandbox::startup::apply_scenario(cfg)) {
			return 1;
		}
	}
	if (cfg.physics_parity) {
		sandbox::startup::run_physics_parity(cfg);
	}
	else {
		sandbox::startup::run_game(cfg.engine);
	}
	return 0;
}
