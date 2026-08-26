export module gse.http:system;

import std;

import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.log;
import gse.math;
import gse.time;

import :client;
import :request;
import :url;

export namespace gse::http {
	struct fetch_request {
		using result_type = result;
		request req;
		channel_promise<result> promise;
	};

	struct throttle_request {
		std::string url;
		time min_interval;
	};

	struct host_throttle {
		time min_interval{};
		clock since_last;
		bool primed = false;
	};

	struct queued_fetch {
		request req;
		std::string host;
		channel_promise<result> promise;
	};

	struct [[= system_state<"Http">{}]] data {
		[[= shared]] std::uint32_t queued = 0;
		[[= shared]] std::uint32_t in_flight = 0;
		[[= shared]] std::uint64_t succeeded = 0;
		[[= shared]] std::uint64_t failed = 0;
		std::unique_ptr<client> client_ptr;
		std::vector<queued_fetch> queue;
		std::flat_map<id, channel_promise<result>> waiting;
		std::unordered_map<std::string, host_throttle> throttles;
	};

	[[= system_run<>{}]]
	auto run(
		data& d,
		channel_read<fetch_request, throttle_request> requests
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

namespace gse::http {
	auto enqueue(
		data& d,
		const fetch_request& req
	) -> void;

	auto dispatch_ready(
		data& d
	) -> void;

	auto collect_completions(
		data& d
	) -> void;
}

auto gse::http::enqueue(data& d, const fetch_request& req) -> void {
	const auto target = parse_url(req.req.url);
	if (!target) {
		req.promise.fulfill(std::unexpected(error::bad_url));
		++d.failed;
		return;
	}

	d.queue.push_back({
		.req = req.req,
		.host = target->host,
		.promise = req.promise,
	});
}

auto gse::http::dispatch_ready(data& d) -> void {
	const std::size_t capacity = d.client_ptr->capacity();
	std::size_t index = 0;

	while (index < d.queue.size() && d.waiting.size() < capacity) {
		queued_fetch& entry = d.queue[index];
		host_throttle& throttle = d.throttles[entry.host];

		const time idle{};
		if (throttle.primed && throttle.min_interval > idle && throttle.since_last.elapsed() < throttle.min_interval) {
			++index;
			continue;
		}

		throttle.primed = true;
		throttle.since_last.reset();

		const id ticket = d.client_ptr->send(std::move(entry.req));
		d.waiting.emplace(ticket, entry.promise);
		d.queue.erase(d.queue.begin() + static_cast<std::ptrdiff_t>(index));
	}
}

auto gse::http::collect_completions(data& d) -> void {
	for (auto& done : d.client_ptr->poll()) {
		const auto it = d.waiting.find(done.ticket);
		if (it == d.waiting.end()) {
			continue;
		}

		if (done.value) {
			++d.succeeded;
		}
		else {
			++d.failed;
			log::println(log::level::warning, log::category::http, "request failed: {}", message(done.value.error()));
		}

		it->second.fulfill(std::move(done.value));
		d.waiting.erase(it);
	}
}

auto gse::http::run(data& d, const channel_read<fetch_request, throttle_request> requests) -> async::task<> {
	if (!d.client_ptr) {
		d.client_ptr = std::make_unique<client>();
	}

	for (const auto& req : requests.of<throttle_request>()) {
		if (const auto target = parse_url(req.url)) {
			d.throttles[target->host].min_interval = req.min_interval;
		}
		else {
			log::println(log::level::error, log::category::http, "throttle target '{}' is not a valid url", req.url);
		}
	}

	for (const auto& req : requests.of<fetch_request>()) {
		enqueue(d, req);
	}

	dispatch_ready(d);
	collect_completions(d);

	d.queued = static_cast<std::uint32_t>(d.queue.size());
	d.in_flight = static_cast<std::uint32_t>(d.waiting.size());

	return {};
}

auto gse::http::shutdown(data& d) -> void {
	d.queue.clear();
	d.waiting.clear();
	d.client_ptr.reset();
}