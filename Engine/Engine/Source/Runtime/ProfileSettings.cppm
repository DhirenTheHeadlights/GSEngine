export module gse.runtime:profile_settings;

import std;

import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.meta;

export namespace gse::profile_settings {
	struct [[= system_state<"ProfileSettings">{}, = settings::category<"Dev">{}]] data {
		[[
			= settings::describe<"Aggregate per-frame profiler samples into rolling averages for the HUD.">{}
		]]
		bool profile_aggregator_enabled = true;

		[[
			= settings::describe<"Retain a rolling ring of recent frame traces so a profile dump can emit the "
									  "worst frames instead of the current one. Costs a per-frame copy of the trace.">{}
		]]
		bool profile_frame_recording = false;

		[[
			= settings::describe<"Frames to discard before the profiler starts accumulating. Boot and "
									  "first-use pipeline warmup produce multi-millisecond frames that would "
									  "otherwise pin every peak for the rest of the run. Editing this re-arms "
									  "the countdown, so it doubles as a way to restart a capture.">{},
			= settings::range<0, 2000>{}
		]]
		int profile_warmup_frames = static_cast<int>(profile::default_warmup_frames);

		bool last_profile_aggregator_enabled = true;
		bool last_profile_frame_recording = false;
		int last_profile_warmup_frames = static_cast<int>(profile::default_warmup_frames);
	};

	[[= system_run<>{}]]
	auto run(
		data& d
	) -> async::task<>;
}

auto gse::profile_settings::run(data& d) -> async::task<> {
	if (d.profile_aggregator_enabled != d.last_profile_aggregator_enabled) {
		profile::set_enabled(d.profile_aggregator_enabled);
		d.last_profile_aggregator_enabled = d.profile_aggregator_enabled;
	}
	if (d.profile_frame_recording != d.last_profile_frame_recording) {
		profile::set_frame_recording(d.profile_frame_recording);
		d.last_profile_frame_recording = d.profile_frame_recording;
	}
	if (d.profile_warmup_frames != d.last_profile_warmup_frames) {
		profile::reset();
		profile::set_warmup_frames(static_cast<std::uint64_t>(std::max(d.profile_warmup_frames, 0)));
		d.last_profile_warmup_frames = d.profile_warmup_frames;
	}

	return {};
}
