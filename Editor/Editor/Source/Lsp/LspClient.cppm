export module ide:lsp_client;

import std;

import :lsp_protocol;

export namespace ide::lsp {
	struct server_config {
		std::filesystem::path executable;
		std::vector<std::string> args;
		std::filesystem::path root_uri;
	};

	struct client {
		struct data {
			std::optional<server_config> active;
			std::vector<diagnostic> diagnostics;
			std::int32_t next_request_id = 1;
			bool initialized = false;
		};
	};
}
