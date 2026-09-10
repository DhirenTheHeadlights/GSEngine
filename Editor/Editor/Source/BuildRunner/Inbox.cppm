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
		std::filesystem::path cwd;
		std::filesystem::path project;
		bool run = false;
	};

	struct result {
		std::string id;
		status outcome = status::ok;
		std::uint32_t owned = 0;
		std::vector<std::string> lines;
	};

	struct presence {
		std::string agent;
		std::string name;
		std::string tree;
	};

	struct hibernate_request {
		std::string id;
		std::string agent;
		std::string prompt;
		std::filesystem::path cwd;
	};

	auto directory() -> std::filesystem::path;

	auto requests_dir() -> std::filesystem::path;

	auto results_dir() -> std::filesystem::path;

	auto hibernate_dir() -> std::filesystem::path;

	auto presence_dir() -> std::filesystem::path;

	auto publish_presence(
		const presence& active
	) -> void;

	auto clear_presence(
		std::string_view agent
	) -> void;

	auto take_presence() -> std::vector<presence>;

	auto peek_hibernations() -> std::vector<hibernate_request>;

	auto peek_requests() -> std::vector<request>;

	auto consume_hibernation(
		std::string_view id
	) -> void;

	auto consume_request(
		std::string_view id
	) -> void;

	auto restore(
		const request& pending
	) -> void;

	auto publish(
		const result& outcome
	) -> void;

	auto withdraw(
		std::string_view id
	) -> void;

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
		bool refs = false;
	};

	auto queries_dir() -> std::filesystem::path;

	auto peek_symbol_queries() -> std::vector<symbol_query>;

	auto consume_symbol_query(
		std::string_view id
	) -> void;
}
