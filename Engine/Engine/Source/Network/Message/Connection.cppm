export module gse.network:connection;

import :message;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::network {
    struct [[= network_message{}]] connection_request {
    };

    struct [[= network_message{}]] connection_accepted {
        id controller_id{};
    };
}
