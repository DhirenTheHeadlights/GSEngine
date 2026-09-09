export module gse.ide.build:inbox;

import std;
import gse;

export namespace gse::ide::build_inbox {
	enum class status : std::uint8_t {
		ok,
		failed,
		waiting,
		rejected,
		aborted,
	};

	struct request {
		std::string id;
		std::string agent;
		std::string target;
		std::string tree;
		std::string profile;
		bool run = false;
	};

	struct result {
		std::string id;
		status outcome = status::ok;
		std::uint32_t owned = 0;
		std::vector<std::string> lines;
	};

	struct hibernate_request {
		std::string id;
		std::string agent;
		std::string prompt;
	};

	auto directory() -> std::filesystem::path;

	auto requests_dir() -> std::filesystem::path;

	auto results_dir() -> std::filesystem::path;

	auto hibernate_dir() -> std::filesystem::path;

	auto take_hibernations() -> std::vector<hibernate_request>;

	auto take_requests() -> std::vector<request>;

	auto restore(
		const request& pending
	) -> void;

	auto publish(
		const result& outcome
	) -> void;

	auto withdraw(
		std::string_view id
	) -> void;
}

	struct symbol_query {
		std::string id;
		std::string agent;
		std::string name;
		std::filesystem::path file;
		std::filesystem::path cwd;
		std::filesystem::path project;
		std::uint32_t sites = 0;
		std::uint32_t lines = 0;
		bool body = true;
	};

	auto queries_dir() -> std::filesystem::path;

	auto peek_symbol_queries() -> std::vector<symbol_query>;

	auto consume_symbol_query(
		std::string_view id
	) -> void;
