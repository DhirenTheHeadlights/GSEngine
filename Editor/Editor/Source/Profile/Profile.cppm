export module gse.ide.profile;

import std;
import gse;

export namespace gse::ide {
	struct view_label {
		char text[32];
	};

	enum class profile_mode {
		flame,
		table,
	};

	enum class profile_source {
		editor,
		game,
	};

	enum class profile_column {
		per_frame [[= view_label{ .text = "per/f us" }]],
		ema [[= view_label{ .text = "avg us" }]],
		peak [[= view_label{ .text = "peak us" }]],
		share [[= view_label{ .text = "% frame" }]],
		calls [[= view_label{ .text = "calls/f" }]],
	};

	struct profile_row {
		gse::id id;
		std::string_view tag;
		gse::profile::sample_time per_frame;
		gse::profile::sample_time ema;
		gse::profile::sample_time last;
		gse::profile::sample_time peak;
		double calls_per_frame = 0.0;
		std::uint64_t sample_count = 0;
		std::uint32_t dominant_tid = 0;
	};

	struct captured_frame {
		std::vector<gse::trace::node> nodes;
		std::vector<std::uint32_t> children;
		std::vector<std::uint32_t> roots;
		std::uint64_t generation = 0;
		gse::time_t<std::uint64_t> origin;
		gse::time_t<double> span;
	};

	struct lane_buckets {
		std::vector<std::uint32_t> tids;
		std::vector<std::uint32_t> offsets;
		std::vector<std::uint32_t> nodes;
	};

	struct profile_view_state {
		profile_mode mode = profile_mode::flame;
		profile_source source = profile_source::editor;
		bool enabled = false;
		std::uint64_t pinned_generation = 0;
		std::uint64_t live_generation = 0;
		gse::interval_timer<> live_timer{ gse::milliseconds(100.f) };
		gse::id selected;
		gse::id hovered;
		profile_column sort_column = profile_column::per_frame;
		bool sort_descending = true;
		std::size_t strip_visible = 0;
		std::size_t worker_offset = 0;
		float detail_width = 0.f;
		bool resizing_detail = false;
		bool wants_resize_cursor = false;
		gse::gui::column_state columns;
		gse::profile::sample_time live_frame_time;
		std::uint32_t live_main_tid = 0;
		gse::gui::dropdown_state source_dropdown;
		std::vector<gse::profile::report_entry> cpu_rows;
		std::vector<gse::profile::report_entry> gpu_rows;
		std::vector<profile_row> cpu_display;
		std::vector<profile_row> gpu_display;
		std::vector<std::uint32_t> depth_scratch;
		std::vector<std::uint32_t> stack_scratch;
		lane_buckets lanes;
	};

	struct profile_capture_request {
		bool enabled = false;
		profile_source source = profile_source::editor;
	};

	struct profile_report_request {
		std::filesystem::path path;
	};

	auto draw_profile_panel(
		gse::gui::builder& ui,
		const rectf& rect,
		profile_view_state& state,
		const std::deque<captured_frame>& frames,
		const gse::profile::report_file& report,
		bool report_loaded,
		gse::channel_writer channels
	) -> void;

	namespace profile_system {
		struct [[= system_state<"Profile">{}]] data {
			[[= shared]] std::deque<captured_frame> frames;
			[[= shared]] gse::profile::report_file report;
			[[= shared]] bool report_loaded = false;
			bool capturing = false;
			profile_source source = profile_source::editor;
			std::uint64_t last_generation = 0;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d
		) -> async::task<>;
	}
}

