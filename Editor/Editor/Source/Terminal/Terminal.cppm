export module gse.ide.terminal:terminal_panel;

import std;
import gse;
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
		auto write(const log::record& rec) -> void override;

		auto write_raw(std::string_view text) -> void override;

		auto flush() -> void override;

		auto drain(std::uint64_t& cursor, std::vector<line>& out) -> void;

		auto sequence() -> std::uint64_t;

	private:
		auto push(log::level lvl, std::string text) -> void;

		std::mutex m_mutex;
		std::deque<line> m_lines;
		std::uint64_t m_next = 0;
	};

	struct instance {
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
		bool interactive = true;
	};

	struct [[= system_state<"Terminal">{}]] data {
		ring_sink* sink = nullptr;
		std::vector<instance> instances;
		std::size_t active = 0;
		std::uint32_t next_number = 1;
		gui::tab_strip_state tab_strip;
		float strip_width = 140.f;
		bool resizing_strip = false;
		std::optional<std::size_t> pending_close;
	};

	[[= system_init{}]]
	auto init(data& d) -> async::task<>;

	[[= system_run<>{}]]
	auto run(context& ctx, data& d, shared_view<build_runner::data> build_d) -> async::task<>;

	auto register_sink(data& d) -> void;

	auto draw_panel(gui::builder& ui, data& d, channel_writer channels, bool building) -> void;
}
