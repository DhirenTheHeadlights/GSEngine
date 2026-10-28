module gse.http:client_platform;

import gse.core;
import gse.log;
import gse.math;
import gse.winhttp;
import std;

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
}

gse::http::scoped_handle::scoped_handle(const winhttp::handle raw) : value(raw) {}

gse::http::scoped_handle::scoped_handle(scoped_handle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}

auto gse::http::scoped_handle::operator=(scoped_handle&& other) noexcept -> scoped_handle& {
	if (this != &other) {
		if (value) {
			::WinHttpCloseHandle(value);
		}
		value = std::exchange(other.value, nullptr);
	}
	return *this;
}

gse::http::scoped_handle::~scoped_handle() {
	if (value) {
		::WinHttpCloseHandle(value);
	}
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

auto gse::http::open_session(const std::string_view user_agent) -> native_session {
	const std::wstring agent = widen(user_agent);
	const native_session handle = ::WinHttpOpen(agent.c_str(), winhttp::access_type_automatic_proxy, nullptr, nullptr, 0);
	if (!handle) {
		log::println(log::level::error, log::category::http, "WinHttpOpen failed: {}", ::GetLastError());
	}
	return handle;
}

auto gse::http::close_session(const native_session handle) -> void {
	::WinHttpCloseHandle(handle);
}

auto gse::http::perform(const native_session session_handle, const request& req, const std::atomic<bool>& cancelled) -> result {
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

	std::ofstream sink;
	if (!req.sink.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(req.sink.parent_path(), ec);
		sink.open(req.sink, std::ios::binary | std::ios::trunc);
		if (!sink) {
			return std::unexpected(error::sink_failed);
		}
	}

	std::string chunk;
	std::uint64_t total = 0;
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
		std::string& target = sink.is_open() ? chunk : out.body;
		const std::size_t offset = sink.is_open() ? 0 : target.size();
		if (!sink.is_open() && offset + available > req.max_body_bytes) {
			return std::unexpected(error::too_large);
		}
		target.resize(offset + available);

		unsigned long taken = 0;
		if (::WinHttpReadData(exchange.value, target.data() + offset, available, &taken) == 0) {
			return std::unexpected(classify(::GetLastError(), error::receive_failed));
		}
		target.resize(offset + taken);
		if (sink.is_open()) {
			sink.write(target.data(), static_cast<std::streamsize>(taken));
			if (!sink) {
				return std::unexpected(error::sink_failed);
			}
		}
		total += taken;
		if (req.received != nullptr) {
			req.received->store(total, std::memory_order_release);
		}
		if (taken == 0) {
			break;
		}
	}

	if (sink.is_open()) {
		sink.close();
		if (!sink) {
			return std::unexpected(error::sink_failed);
		}
	}

	return out;
}
