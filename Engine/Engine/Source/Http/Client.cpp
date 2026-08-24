module gse.http:client_impl;

import std;

import gse.core;
import gse.log;
import gse.math;
import gse.winhttp;

import :client;
import :request;
import :url;

namespace gse::http {
	struct scoped_handle : non_copyable {
		winhttp::handle value = nullptr;

		scoped_handle() = default;

		explicit scoped_handle(
			winhttp::handle raw
		);

		~scoped_handle();

		scoped_handle(
			scoped_handle&& other
		) noexcept;

		auto operator=(
			scoped_handle&& other
		) noexcept -> scoped_handle&;

		explicit operator bool() const;
	};

	auto classify(
		unsigned long code,
		error fallback
	) -> error;

	auto read_status(
		winhttp::handle exchange,
		std::uint16_t& out
	) -> bool;

	auto read_headers(
		winhttp::handle exchange
	) -> std::vector<header>;

	auto perform(
		winhttp::handle session_handle,
		const request& req,
		const std::atomic<bool>& cancelled
	) -> result;
}

struct gse::http::client::session {
	struct pending {
		id ticket;
		http::request req;
	};

	winhttp::handle handle = nullptr;
	std::mutex mutex;
	std::condition_variable_any ready;
	std::condition_variable drained;
	std::deque<pending> queue;
	std::vector<completion> completed;
	std::size_t active = 0;
	std::atomic<std::uint64_t> next{ 1 };
	std::atomic<std::size_t> outstanding{ 0 };
	std::atomic<bool> abandoned{ false };
	std::vector<std::jthread> workers;

	~session();

	auto start(
		std::size_t worker_count
	) -> void;

	auto pump(
		const std::stop_token& stop
	) -> void;

	auto finish(
		id ticket,
		result value
	) -> void;
};

gse::http::scoped_handle::scoped_handle(const winhttp::handle raw) : value(raw) {}

gse::http::scoped_handle::scoped_handle(scoped_handle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}

auto gse::http::scoped_handle::operator=(scoped_handle&& other) noexcept -> scoped_handle& {
	if (this != &other) {
		if (value) {
#ifdef _WIN32
			::WinHttpCloseHandle(value);
#endif
		}
		value = std::exchange(other.value, nullptr);
	}
	return *this;
}

gse::http::scoped_handle::~scoped_handle() {
#ifdef _WIN32
	if (value) {
		::WinHttpCloseHandle(value);
	}
#endif
}

gse::http::scoped_handle::operator bool() const {
	return value != nullptr;
}

auto gse::http::classify(const unsigned long code, const error fallback) -> error {
	if (code == winhttp::error_timeout) {
		return error::timed_out;
	}
	if (code == winhttp::error_operation_cancelled) {
		return error::cancelled;
	}
	if (code == winhttp::error_cannot_connect || code == winhttp::error_name_not_resolved) {
		return error::connect_failed;
	}
	return fallback;
}

#ifdef _WIN32

auto gse::http::read_status(const winhttp::handle exchange, std::uint16_t& out) -> bool {
	unsigned long status = 0;
	unsigned long size = sizeof(status);
	if (::WinHttpQueryHeaders(exchange, winhttp::query_status_code | winhttp::query_flag_number, nullptr, &status, &size, nullptr) == 0) {
		return false;
	}
	out = static_cast<std::uint16_t>(status);
	return true;
}

auto gse::http::read_headers(const winhttp::handle exchange) -> std::vector<header> {
	unsigned long size = 0;
	::WinHttpQueryHeaders(exchange, winhttp::query_raw_headers_crlf, nullptr, nullptr, &size, nullptr);
	if (::GetLastError() != winhttp::error_insufficient_buffer || size == 0) {
		return {};
	}

	std::wstring raw(size / sizeof(wchar_t) + 1, L'\0');
	if (::WinHttpQueryHeaders(exchange, winhttp::query_raw_headers_crlf, nullptr, raw.data(), &size, nullptr) == 0) {
		return {};
	}
	raw.resize(size / sizeof(wchar_t));

	const std::string flat = narrow(raw);
	std::vector<header> out;

	for (const auto line : std::views::split(std::string_view(flat), std::string_view("\r\n"))) {
		const std::string_view entry(line.begin(), line.end());
		const auto colon = entry.find(':');
		if (colon == std::string_view::npos) {
			continue;
		}
		std::string_view value = entry.substr(colon + 1);
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
			value.remove_prefix(1);
		}
		out.push_back({
			.name = std::string(entry.substr(0, colon)),
			.value = std::string(value),
		});
	}

	return out;
}

auto gse::http::perform(const winhttp::handle session_handle, const request& req, const std::atomic<bool>& cancelled) -> result {
	if (!session_handle) {
		return std::unexpected(error::session_failed);
	}

	const auto target = parse_url(req.url);
	if (!target) {
		return std::unexpected(error::bad_url);
	}

	const std::wstring host = widen(target->host);
	const scoped_handle connection(::WinHttpConnect(session_handle, host.c_str(), target->port, 0));
	if (!connection) {
		return std::unexpected(classify(::GetLastError(), error::connect_failed));
	}

	const method_info selected = method_of(req.verb);
	const std::wstring verb = widen(selected.verb);
	const std::wstring path = widen(target->target);
	const scoped_handle exchange(::WinHttpOpenRequest(connection.value, verb.c_str(), path.c_str(), nullptr, nullptr, nullptr, target->secure ? winhttp::flag_secure : 0));
	if (!exchange) {
		return std::unexpected(classify(::GetLastError(), error::request_failed));
	}

	const auto budget = static_cast<int>(req.timeout.as<milliseconds>());
	::WinHttpSetTimeouts(exchange.value, budget, budget, budget, budget);

	unsigned long decompression = winhttp::decompression_flag_all;
	::WinHttpSetOption(exchange.value, winhttp::option_decompression, &decompression, sizeof(decompression));

	unsigned long protocols = winhttp::secure_protocol_tls1_2 | winhttp::secure_protocol_tls1_3;
	::WinHttpSetOption(exchange.value, winhttp::option_secure_protocols, &protocols, sizeof(protocols));

	std::string header_block;
	for (const auto& [name, value] : req.headers) {
		header_block.append(name);
		header_block.append(": ");
		header_block.append(value);
		header_block.append("\r\n");
	}
	if (!header_block.empty()) {
		const std::wstring wide = widen(header_block);
		::WinHttpAddRequestHeaders(exchange.value, wide.c_str(), ~0ul, winhttp::addreq_flag_add | winhttp::addreq_flag_replace);
	}

	if (cancelled.load(std::memory_order_acquire)) {
		return std::unexpected(error::cancelled);
	}

	const auto body_size = static_cast<unsigned long>(req.body.size());
	void* payload = req.body.empty() ? nullptr : const_cast<char*>(req.body.data());
	if (::WinHttpSendRequest(exchange.value, nullptr, 0, payload, body_size, body_size, 0) == 0) {
		return std::unexpected(classify(::GetLastError(), error::send_failed));
	}

	if (::WinHttpReceiveResponse(exchange.value, nullptr) == 0) {
		return std::unexpected(classify(::GetLastError(), error::receive_failed));
	}

	response out;
	if (!read_status(exchange.value, out.status)) {
		return std::unexpected(error::receive_failed);
	}
	out.headers = read_headers(exchange.value);

	while (true) {
		if (cancelled.load(std::memory_order_acquire)) {
			return std::unexpected(error::cancelled);
		}

		unsigned long available = 0;
		if (::WinHttpQueryDataAvailable(exchange.value, &available) == 0) {
			return std::unexpected(classify(::GetLastError(), error::receive_failed));
		}
		if (available == 0) {
			break;
		}
		if (out.body.size() + available > req.max_body_bytes) {
			return std::unexpected(error::too_large);
		}

		const std::size_t offset = out.body.size();
		out.body.resize(offset + available);

		unsigned long taken = 0;
		if (::WinHttpReadData(exchange.value, out.body.data() + offset, available, &taken) == 0) {
			return std::unexpected(classify(::GetLastError(), error::receive_failed));
		}
		out.body.resize(offset + taken);
		if (taken == 0) {
			break;
		}
	}

	return out;
}

#else

auto gse::http::read_status(winhttp::handle, std::uint16_t&) -> bool {
	return false;
}

auto gse::http::read_headers(winhttp::handle) -> std::vector<header> {
	return {};
}

auto gse::http::perform(winhttp::handle, const request&, const std::atomic<bool>&) -> result {
	return std::unexpected(error::unsupported_platform);
}

#endif

gse::http::client::session::~session() {
	{
		std::lock_guard lock(mutex);
		abandoned.store(true, std::memory_order_release);
		outstanding.fetch_sub(queue.size(), std::memory_order_relaxed);
		queue.clear();

#ifdef _WIN32
		if (handle) {
			::WinHttpCloseHandle(handle);
		}
#endif
	}

	for (auto& worker : workers) {
		worker.request_stop();
	}
	ready.notify_all();

	{
		std::unique_lock lock(mutex);
		drained.wait(lock, [this] {
			return active == 0;
		});
		handle = nullptr;
	}

	workers.clear();
}

auto gse::http::client::session::start(const std::size_t worker_count) -> void {
	workers.reserve(worker_count);
	for (std::size_t i = 0; i < worker_count; ++i) {
		workers.emplace_back([this, i](std::stop_token stop) {
			log::name_thread(log::thread_role::http, i);
			pump(stop);
		});
	}
}

auto gse::http::client::session::pump(const std::stop_token& stop) -> void {
	while (!stop.stop_requested()) {
		pending job;

		{
			std::unique_lock lock(mutex);
			ready.wait(lock, stop, [this] {
				return !queue.empty();
			});

			if (abandoned.load(std::memory_order_acquire) || queue.empty()) {
				return;
			}

			job = std::move(queue.front());
			queue.pop_front();
			++active;
		}

		result value = perform(handle, job.req, abandoned);

		{
			std::lock_guard lock(mutex);
			--active;
		}
		drained.notify_all();

		finish(job.ticket, std::move(value));
	}
}

auto gse::http::client::session::finish(const id ticket, result value) -> void {
	outstanding.fetch_sub(1, std::memory_order_relaxed);

	std::lock_guard lock(mutex);
	if (abandoned.load(std::memory_order_acquire)) {
		return;
	}
	completed.push_back({
		.ticket = ticket,
		.value = std::move(value),
	});
}

gse::http::client::client() : client("GSEngine", default_worker_count) {}

gse::http::client::client(const std::string_view user_agent) : client(user_agent, default_worker_count) {}

gse::http::client::client(const std::string_view user_agent, const std::size_t worker_count) : m_session(std::make_shared<session>()) {
#ifdef _WIN32
	const std::wstring agent = widen(user_agent);
	m_session->handle = ::WinHttpOpen(agent.c_str(), winhttp::access_type_automatic_proxy, nullptr, nullptr, 0);
	if (!m_session->handle) {
		log::println(log::level::error, log::category::http, "WinHttpOpen failed: {}", ::GetLastError());
	}
#else
	(void)user_agent;
	log::println(log::level::warning, log::category::http, "no http backend on this platform; every request will fail");
#endif

	m_session->start(std::max<std::size_t>(1, worker_count));
}

gse::http::client::client(client&& other) noexcept = default;

auto gse::http::client::operator=(client&& other) noexcept -> client& = default;

gse::http::client::~client() = default;

auto gse::http::client::send(request req) -> id {
	if (!m_session) {
		return {};
	}

	const id ticket = generate_temp_id(m_session->next.fetch_add(1, std::memory_order_relaxed));

	{
		std::lock_guard lock(m_session->mutex);
		if (m_session->abandoned.load(std::memory_order_acquire)) {
			return {};
		}
		m_session->queue.push_back({
			.ticket = ticket,
			.req = std::move(req),
		});
	}

	m_session->outstanding.fetch_add(1, std::memory_order_relaxed);
	m_session->ready.notify_one();

	return ticket;
}

auto gse::http::client::capacity() const -> std::size_t {
	return m_session ? m_session->workers.size() : 0;
}

auto gse::http::client::poll() -> std::vector<completion> {
	if (!m_session) {
		return {};
	}

	std::vector<completion> out;
	std::lock_guard lock(m_session->mutex);
	out.swap(m_session->completed);
	return out;
}

auto gse::http::client::in_flight() const -> std::size_t {
	return m_session ? m_session->outstanding.load(std::memory_order_relaxed) : 0;
}

auto gse::http::client::valid() const -> bool {
	return m_session && m_session->handle != nullptr;
}
