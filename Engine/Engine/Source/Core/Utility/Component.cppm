export module gse.utility:component;

import std;

import :id;

export namespace gse {
    struct no_data {};

    template <typename Data = no_data>
    struct component : identifiable_owned_only_uuid, Data {
        using params = Data;

        component() = default;
        explicit component(const id& owner_id, const params& p = {}) : identifiable_owned_only_uuid(owner_id), Data(p) {}
    };
}