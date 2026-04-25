export module gse.network:notify_scene_change;

import std;

import :message;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::network {
    struct [[= network_message{}]] notify_scene_change {
        id scene_id{};
    };
}
