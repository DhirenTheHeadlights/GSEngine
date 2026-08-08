module gse.runtime:bench_impl;

import std;

import :bench;
import :engine;
import :scene;
import :state_dump;
import :world_system;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.physics;
import gse.log;
import gse.config;
import gse.fs;
import gse.scenario;

namespace gse {
	constexpr int bench_settle_frame_limit = 6000;

	auto world_body_order(
		std::span<const id> owners
	) -> std::vector<std::size_t>;

	auto compare_against_baseline(
		const bench_config& config,
		const std::filesystem::path& run_dir,
		std::string_view stamp,
		const profile::run_summary& summary,
		const std::filesystem::path& profile_path
	) -> void;
}

auto gse::world_populated(registry& reg) -> bool {
	return !reg.owner_ids<physics::transform_component>().empty();
}

auto gse::world_body_order(const std::span<const id> owners) -> std::vector<std::size_t> {
	std::vector<std::size_t> order(owners.size());
	std::ranges::iota(order, std::size_t{ 0 });
	std::ranges::sort(
		order,
		[owners](const std::size_t a, const std::size_t b) {
			return owners[a].number() < owners[b].number();
		}
	);
	return order;
}

auto gse::world_state_hash(registry& reg) -> std::uint64_t {
	const auto owners = reg.owner_ids<physics::transform_component>();
	const auto transforms = reg.components<physics::transform_component>();

	const auto order = world_body_order(owners);

	std::ostringstream buffer;
	binary_writer writer(buffer);

	for (const std::size_t index : order) {
		const gse::id owner = owners[index];
		const std::uint64_t number = owner.number();
		writer & number;
		writer & transforms[index];
		if (const auto* motion = reg.try_component<physics::motion_component>(owner)) {
			writer & *motion;
		}
	}

	return stable_id(buffer.view());
}

auto gse::write_state_frame(registry& reg, std::ostream& out, const std::uint32_t frame) -> void {
	const auto owners = reg.owner_ids<physics::transform_component>();
	const auto transforms = reg.components<physics::transform_component>();

	const auto order = world_body_order(owners);

	binary_writer writer(out);

	for (const std::size_t index : order) {
		const gse::id owner = owners[index];
		const auto* motion = reg.try_component<physics::motion_component>(owner);
		state_dump_record record{
			.owner = owner.number(),
			.frame = frame,
			.position = transforms[index].position,
			.velocity = motion ? motion->current_velocity : vec3<velocity>{},
		};
		writer & record;
	}
}

auto gse::bench_world_ready(const bench_config& config, engine& e) -> bool {
	return e.all_settled() && (config.scene.empty() || world_populated(e.registry()));
}

auto gse::drive_scenario(const bench_config& config, bench_state& state, engine& e) -> void {
	if (!config.scenario_body) {
		return;
	}

	if (!state.scenario_started) {
		state.scenario_started = true;
		state.scenario_ctx.emplace(e.make_channel_writer(), &state.gate);
		state.scenario_task = config.scenario_body(*state.scenario_ctx);
		state.scenario_task.start();
		return;
	}

	state.scenario_ctx->advance_frame();

	if (!state.gate.armed) {
		return;
	}

	if (state.gate.until_settled) {
		if (!bench_world_ready(config, e)) {
			return;
		}
	}
	else {
		if (state.gate.frames_remaining > 0) {
			--state.gate.frames_remaining;
		}
		if (state.gate.frames_remaining > 0) {
			return;
		}
	}

	state.gate.armed = false;
	if (!async::resume_checked(state.gate.resume)) {
		log::println(log::level::error, log::category::general, "bench: the scenario coroutine frame went stale; the rest of the scripted workload will not run");
	}
}

auto gse::compare_against_baseline(const bench_config& config, const std::filesystem::path& run_dir, const std::string_view stamp, const profile::run_summary& summary, const std::filesystem::path& profile_path) -> void {
	const std::filesystem::path baseline_path = run_dir / "baseline.gsprof";

	if (config.update_baseline) {
		std::error_code ec;
		std::filesystem::copy_file(profile_path, baseline_path, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec) {
			log::println(log::level::error, log::category::general, "bench: could not record the baseline at {}: {}", baseline_path.display_string(), ec.message());
		}
		else {
			log::println(log::category::general, "bench: recorded this run as the baseline at {}", baseline_path.display_string());
		}
		return;
	}

	if (!std::filesystem::exists(baseline_path)) {
		log::println(log::category::general, "bench: no baseline at {}; rerun with --engine-bench-update-baseline to record this run as one", baseline_path.display_string());
		return;
	}

	const auto baseline_file = profile::load_report(baseline_path);
	if (!baseline_file) {
		log::println(log::level::error, log::category::general, "bench: the baseline at {} could not be read, so this run was not compared", baseline_path.display_string());
		return;
	}

	const profile::run_summary baseline = profile::summarize(*baseline_file);
	const profile::run_diff diff = profile::diff_summaries(baseline, summary, config.regression_threshold);
	const std::filesystem::path diff_path = run_dir / std::format("{}.diff.txt", stamp);
	profile::write_diff(diff, diff_path);

	log::println(
		diff.regressed ? log::level::error : log::level::info,
		log::category::general,
		"bench: frame p50 {:.3f:ms} -> {:.3f:ms} ({:.3f}x against a {:.3f}x threshold), p95 {:.3f:ms} -> {:.3f:ms} ({:.3f}x); {} — see {}",
		diff.baseline_frame_p50,
		diff.current_frame_p50,
		diff.frame_p50_ratio,
		diff.threshold,
		diff.baseline_frame_p95,
		diff.current_frame_p95,
		diff.frame_p95_ratio,
		diff.regressed ? "REGRESSED" : "within threshold",
		diff_path.display_string()
	);
}

auto gse::begin_bench(const bench_config& config) -> void {
	system_clock::set_fixed_step_override(1);
	profile::set_enabled(true);
	profile::set_frame_recording(true);
	profile::set_recording_capacity(static_cast<std::size_t>(std::max(config.frames, 1)));

	log::println(
		log::category::general,
		"bench: fixed-step clock engaged, {} warmup frames then {} measured frames",
		config.warmup_frames,
		config.frames
	);

	if (!config.scenario.empty() && !config.scenario_body) {
		log::println(
			log::level::error,
			log::category::general,
			"bench: scenario '{}' was named but no body was resolved for it; the application owns that lookup, so this run measures an unscripted world",
			config.scenario
		);
	}
}

auto gse::step_bench(const bench_config& config, bench_state& state, engine& e) -> bool {
	drive_scenario(config, state, e);

	switch (state.phase) {
		case bench_phase::settling: {
			if (!state.scene_requested) {
				state.scene_requested = true;
				if (!config.scene.empty()) {
					activate_scene(e.world(), find_or_generate_id(config.scene));
					log::println(log::category::general, "bench: requested scene '{}'", config.scene);
				}
			}

			if (bench_world_ready(config, e)) {
				state.phase = bench_phase::warmup;
				state.frames_in_phase = 0;
				log::println(log::category::general, "bench: world settled, warming up");
				break;
			}

			if (++state.frames_in_phase >= bench_settle_frame_limit) {
				log::println(
					log::level::error,
					log::category::general,
					"bench: world never became ready within {} frames (scene '{}' populated={}); aborting without a measurement",
					bench_settle_frame_limit,
					config.scene,
					world_populated(e.registry())
				);
				state.phase = bench_phase::done;
				return true;
			}
			break;
		}
		case bench_phase::warmup: {
			if (++state.frames_in_phase >= config.warmup_frames) {
				profile::reset();
				state.phase = bench_phase::measuring;
				state.frames_in_phase = 0;
				if (!config.state_dump_out.empty()) {
					state.state_dump.open(config.state_dump_out, std::ios::binary | std::ios::trunc);
					if (state.state_dump) {
						binary_writer header(state.state_dump, state_dump_magic, state_dump_version);
						log::println(log::category::general, "bench: recording per-frame world state to {}", config.state_dump_out);
					}
					else {
						log::println(log::level::error, log::category::general, "bench: could not open {} for the state dump; this run records timings only", config.state_dump_out);
					}
				}
				log::println(log::category::general, "bench: warmup complete, measuring");
			}
			break;
		}
		case bench_phase::measuring: {
			if (state.state_dump.is_open()) {
				write_state_frame(e.registry(), state.state_dump, static_cast<std::uint32_t>(state.frames_in_phase));
			}
			if (++state.frames_in_phase >= config.frames) {
				state.phase = bench_phase::done;
				state.state_dump.close();
				finish_bench(config, state, e);
				return true;
			}
			break;
		}
		case bench_phase::done: {
			break;
		}
	}

	return false;
}

auto gse::finish_bench(const bench_config& config, const bench_state& state, engine& e) -> void {
	if (config.scenario_body && !state.scenario_task.done()) {
		log::println(
			log::level::warning,
			log::category::general,
			"bench: the frame budget expired while the scenario was still running, so part of the scripted workload never happened"
		);
	}

	const std::string stem = config.scenario.empty()
		? std::format("{}_bench", config::executable_stem())
		: config.scenario;
	const std::filesystem::path run_dir = config::profile_dir() / "bench" / stem;
	const std::string stamp = system_clock::timestamp_filename();
	const std::filesystem::path profile_path = config.profile_out.empty()
		? run_dir / std::format("{}.gsprof", stamp)
		: std::filesystem::path(config.profile_out);
	const std::filesystem::path summary_path = config.summary_out.empty()
		? run_dir / std::format("{}.txt", stamp)
		: std::filesystem::path(config.summary_out);

	const profile::report_file file = profile::build_report_file();
	const profile::run_summary summary = profile::summarize(file);
	profile::write_summary(summary, summary_path);
	profile::dump_report(profile_path);
	compare_against_baseline(config, run_dir, stamp, summary, profile_path);

	if (summary.frames == 0) {
		log::println(
			log::level::error,
			log::category::general,
			"bench: {} frames were recorded, so every percentile below is zero by absence rather than by measurement; the world-state hash is still valid but no timing from this run is",
			summary.frames
		);
	}

	log::println(
		log::category::general,
		"bench: {} frames measured, p50 {:.3f:ms} p95 {:.3f:ms} p99 {:.3f:ms}, world-state hash {:#018x}",
		summary.frames,
		summary.frame_p50,
		summary.frame_p95,
		summary.frame_p99,
		world_state_hash(e.registry())
	);
	log::println(log::category::general, "bench: wrote {} and {}", profile_path.display_string(), summary_path.display_string());
}
