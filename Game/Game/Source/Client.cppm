export module gs:client;

import gse;

export namespace gs::client_system {
	[[= gse::system_init{}]]
	auto init(
		gse::context& ctx
	) -> gse::async::task<>;
}