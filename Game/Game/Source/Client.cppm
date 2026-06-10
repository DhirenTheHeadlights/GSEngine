export module gs:client;

import gse;

export namespace gs {
	struct client_system {
		static auto init(
			gse::context& ctx
		) -> gse::async::task<>;
	};
}
