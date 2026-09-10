export module gse.network:connection;

import gse.core;

import :message;

export namespace gse::network {
	struct [[= network_message{}]] connection_request {};

	struct [[= network_message{}]] connection_accepted {
		id controller_id{};
	};
}