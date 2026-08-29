export module gse.ide.terminal:terminal_panel;

import std;
import gse;
import gse.ide.agent;
import gse.ide.build;
import gse.ide.navigation;
import gse.win32;

export namespace gse::ide::terminal {
	constexpr std::string_view panel_name = "Terminal";

	using command_runner = spawn::output_stream;

	struct line {
		std::uint64_t seq;
		log::level lvl;
		std::string text;
	};

	class ring_sink : public log::sink {
	public:
		~ring_sink() override;

		auto write(
			const log::record& rec
		) -> void override;

		auto write_raw(
			std::string_view text
		) -> void override;

		auto flush() -> void override;

		auto drain(
			std::uint64_t& cursor,
			std::vector<line>& out
		) -> void;

		auto sequence() -> std::uint64_t;

	private:
		auto push(
			log::level lvl,
			std::string text
		) -> void;

		std::mutex m_mutex;
		std::deque<line> m_lines;
		std::uint64_t m_next = 0;
	};

	struct dispatch_marker {
		std::uint32_t line = 0;
		agent::blame_offer offer;
	};

	struct instance {
		id instance_id;
		id input_id;
		id log_id;
		id tail_id;
		std::string name;
		bool follows_log = false;
		std::uint64_t cursor = 0;
		gui::text_buffer buffer;
		std::vector<log::level> line_levels;
		std::vector<gui::text_span> spans;
		gui::text_area_state view;
		std::string input;
		gui::text_input_state input_state;
		std::shared_ptr<command_runner> runner;
		std::jthread worker;
		std::vector<dispatch_marker> dispatches;
		bool interactive = true;
		build_runner::stream_kind kind = build_runner::stream_kind::none;
	};

	struct [[= system_state<"Terminal">{}]] data {
		ring_sink* sink = nullptr;
		std::vector<instance> instances;
		id active;
		std::uint32_t next_number = 1;
		std::uint64_t next_id = 1;
		std::string prompt;
		std::vector<line> fresh;
		std::vector<agent::blame_offer> offers;
		std::vector<gui::text_underline> underlines;
		std::vector<gui::tab_desc> tab_descs;
		gui::tab_strip_state tab_strip;
		float strip_width = 140.f;
		gui::layout::split_drag_state resizing_strip;
		std::optional<id> pending_close;
	};

	[[= system_init{}]]
	auto init(
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<build_runner::stream_opened, agent::blame_offer> stream_in,
		channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, gui::menu_content, jump_to_request, set_cursor_shape_request> ui_out,
		shared_view<build_runner::data> build_d
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;

	auto register_sink(
		data& d
	) -> void;

	auto draw_panel(
		gui::builder& ui,
		data& d,
		channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels,
		bool building
	) -> void;
}
