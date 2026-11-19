module gse.http:client_impl;

import gse.concurrency;
import gse.core;
import std;

import :client;
import :request;

struct gse::http::client::session {
	native_session handle = nullptr;
	std::mutex mutex;
	std::vector<completion> completed;
	std::atomic<std::uint64_t> next{ 1 };
	std::atomic<std::size_t> outstanding{ 0 };
	std::atomic<bool> abandoned{ false };

	auto run(
		id ticket,
		const request& req
	) -> void;

	auto abandon() -> void;
};

auto gse::http::client::session::run(const id ticket, const request& req) -> void {
	native_session session_handle = nullptr;
	{
		std::lock_guard _(mutex);
		session_handle = handle;
	}
	result value = perform(session_handle, req, abandoned);
	outstanding.fetch_sub(1, std::memory_order_relaxed);

	std::lock_guard _(mutex);
	if (abandoned.load(std::memory_order_acquire)) {
		return;
	}
	completed.push_back({
		.ticket = ticket,
		.value = std::move(value),
	});
}

auto gse::http::client::session::abandon() -> void {
	std::lock_guard _(mutex);
	abandoned.store(true, std::memory_order_release);
	if (handle) {
		close_session(handle);
	}
	handle = nullptr;
}

gse::http::client::client() : client("GSEngine") {}

gse::http::client::client(const std::string_view user_agent) : m_session(std::make_shared<session>()) {
	m_session->handle = open_session(user_agent);
}

gse::http::client::client(client&& other) noexcept = default;

auto gse::http::client::operator=(client&& other) noexcept -> client& = default;

gse::http::client::~client() {
	if (m_session) {
		m_session->abandon();
	}
}

auto gse::http::client::send(request req) -> id {
	if (!m_session) {
		return {};
	}

	const id ticket = generate_temp_id(m_session->next.fetch_add(1, std::memory_order_relaxed));
	m_session->outstanding.fetch_add(1, std::memory_order_relaxed);
	task::post_io(
		[session = m_session, ticket, req = std::move(req)] {
			session->run(ticket, req);
		},
		trace_id<"http::request">()
	);

	return ticket;
}

auto gse::http::client::poll() -> std::vector<completion> {
	if (!m_session) {
		return {};
	}

	std::vector<completion> out;
	std::lock_guard _(m_session->mutex);
	out.swap(m_session->completed);
	return out;
}

auto gse::http::client::in_flight() const -> std::size_t {
	return m_session ? m_session->outstanding.load(std::memory_order_relaxed) : 0;
}

auto gse::http::client::capacity() const -> std::size_t {
	return m_session ? max_concurrent_requests : 0;
}

auto gse::http::client::valid() const -> bool {
	return m_session && m_session->handle != nullptr;
}
