export module gse.assets:asset_state;

import std;

import gse.core;
import gse.ecs;
import gse.fs;
import gse.concurrency;
import gse.log;

import :resource_loader;

export namespace gse::asset {
	struct hot_reload_request {
		bool enabled = false;
	};

	struct [[= gse::system_state<"Asset">{}]] data {
		[[= gse::shared]] std::unordered_map<id, std::unique_ptr<resource::loader_base>> resource_loaders;
		file_watcher watcher;
		std::function<void()> enable_hot_reload_fn;
		std::function<void()> disable_hot_reload_fn;
		bool hot_reload_enabled = false;
		channel_writer* channels = nullptr;
	};

	struct load_ctx {
		data& assets;
		channel_writer& channels;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		data& d
	) -> async::task<>;

	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		data& d
	) -> async::task<>;

	[[= gse::system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

auto gse::asset::init(context& ctx, data& d) -> async::task<> {
	d.channels = &ctx.channels;
	return {};
}

auto gse::asset::run(context& ctx, data& d) -> async::task<> {
	for (const auto& loader : std::views::values(d.resource_loaders)) {
		loader->flush();
	}

	for (const auto& request : ctx.read_channel<hot_reload_request>()) {
		if (request.enabled == d.hot_reload_enabled) {
			continue;
		}
		if (request.enabled) {
			if (d.enable_hot_reload_fn) {
				d.enable_hot_reload_fn();
			}
			log::println(log::category::assets, "Hot reload enabled");
		}
		else {
			if (d.disable_hot_reload_fn) {
				d.disable_hot_reload_fn();
			}
			log::println(log::category::assets, "Hot reload disabled");
		}
		d.hot_reload_enabled = request.enabled;
	}

	d.watcher.poll();
	return {};
}

auto gse::asset::shutdown(data& d) -> void {
	gse::task::wait_idle();
	d.resource_loaders.clear();
	d.channels = nullptr;
}
