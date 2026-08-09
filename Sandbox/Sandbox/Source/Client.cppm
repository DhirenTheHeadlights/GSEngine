export module sandbox:client;

import gse;

export namespace sandbox::client_system {
	[[= gse::system_init{}]]
	auto init(
		gse::context& ctx,
		gse::channel_write<gse::network::clear_providers_request, gse::network::add_provider_request, gse::network::refresh_servers_request> net_out
	) -> gse::async::task<>;
}