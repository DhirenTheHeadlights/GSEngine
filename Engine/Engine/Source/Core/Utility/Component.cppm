export module gse.utility:component;

import std;

import :id;

export namespace gse {
    struct no_data {};
	class registry;

    template <typename Data = no_data>
    struct component : identifiable_owned_only_uuid, Data {
        using params = Data;

        component() = default;
        explicit component(const id& owner_id, const params& p = {}) : identifiable_owned_only_uuid(owner_id), Data(p) {}

		virtual ~component() = default;

		virtual auto on_registry(registry* reg) -> void { (void)reg; }
    };
}