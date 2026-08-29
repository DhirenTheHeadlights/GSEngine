export module gse.ide.profile;

import std;
import gse;

export namespace gse::ide {
	struct profile_row {
		gse::id id;
		std::string_view tag;
		profile::sample_time per_frame;
		profile::sample_time ema;
		profile::sample_time last;
		profile::sample_time peak;
		double calls_per_frame = 0.0;
		std::uint64_t sample_count = 0;
		std::uint32_t dominant_tid = 0;
	};

	struct view_label {
		char text[32];
		bool dimensioned = false;
		profile::sample_time profile_row::* duration = &profile_row::per_frame;
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
		per_frame [[= view_label{ .text = "per/f", .dimensioned = true }]],
		ema [[= view_label{
			.text = "avg",
			.dimensioned = true,
			.duration = &profile_row::ema,
		}]],
		peak [[= view_label{
			.text = "peak",
			.dimensioned = true,
			.duration = &profile_row::peak,
		}]],
		share [[= view_label{ .text = "% frame" }]],
		calls [[= view_label{ .text = "calls/f" }]],
	};

	struct captured_frame {
		std::vector<trace::node> nodes;
		std::vector<std::uint32_t> children;
		std::vector<std::uint32_t> roots;
		std::uint64_t generation = 0;
		time_t<std::uint64_t> origin;
		time_t<double> span;
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
		interval_timer<> live_timer{ milliseconds(100.f) };
		id selected;
		id hovered;
		profile_column sort_column = profile_column::per_frame;
		bool sort_descending = true;
		std::size_t strip_visible = 0;
		std::size_t worker_offset = 0;
		float detail_width = 0.f;
		gui::layout::split_drag_state resizing_detail;
		bool wants_resize_cursor = false;
		gui::column_state columns;
		profile::sample_time live_frame_time;
		std::uint32_t live_main_tid = 0;
		std::string_view time_unit = std::string_view(microseconds.unit_name);
		gui::dropdown_state source_dropdown;
		gui::dropdown_state unit_dropdown;
		std::vector<profile::report_entry> cpu_rows;
		std::vector<profile::report_entry> gpu_rows;
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
		gui::builder& ui,
		const rectf& rect,
		profile_view_state& state,
		const std::deque<captured_frame>& frames,
		const profile::report_file& report,
		bool report_loaded,
		channel_write<set_cursor_shape_request> channels
	) -> void;

	namespace profile_system {
		struct [[= system_state<"Profile">{}]] data {
			[[= shared]] std::deque<captured_frame> frames;
			[[= shared]] profile::report_file report;
			[[= shared]] bool report_loaded = false;
			bool capturing = false;
			profile_source source = profile_source::editor;
			std::uint64_t last_generation = 0;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<profile_capture_request, profile_report_request> requests_in
		) -> async::task<>;
	}
}
