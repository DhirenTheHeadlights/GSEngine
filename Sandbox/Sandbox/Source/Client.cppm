export module sandbox:client;

import gse;

export namespace sandbox::client_system {
	[[= gse::system_init{}]]
	auto init(
		gse::context& ctx
	) -> gse::async::task<>;
}