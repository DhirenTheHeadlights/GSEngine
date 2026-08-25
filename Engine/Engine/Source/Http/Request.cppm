export module gse.http:request;

import std;

import gse.math;
import gse.meta;

export namespace gse::http {
	struct method_info {
		char verb[8];
		bool sends_body = false;
	};

	enum struct method : std::uint8_t {
		get [[= method_info{
			.verb = "GET",
		}]],
		head [[= method_info{
			.verb = "HEAD",
		}]],
		post [[= method_info{
			.verb = "POST",
			.sends_body = true,
		}]],
		put [[= method_info{
			.verb = "PUT",
			.sends_body = true,
		}]],
		patch [[= method_info{
			.verb = "PATCH",
			.sends_body = true,
		}]],
		remove [[= method_info{
			.verb = "DELETE",
		}]]
	};

	struct error_info {
		char message[56];
		bool retryable = false;
	};

	enum struct error : std::uint8_t {
		bad_url [[= error_info{
			.message = "malformed url",
		}]],
		unsupported_scheme [[= error_info{
			.message = "unsupported url scheme",
		}]],
		unsupported_platform [[= error_info{
			.message = "no http backend on this platform",
		}]],
		session_failed [[= error_info{
			.message = "could not open an http session",
		}]],
		connect_failed [[= error_info{
			.message = "could not connect to host",
			.retryable = true,
		}]],
		request_failed [[= error_info{
			.message = "could not open the request",
		}]],
		send_failed [[= error_info{
			.message = "could not send the request",
			.retryable = true,
		}]],
		receive_failed [[= error_info{
			.message = "could not read the response",
			.retryable = true,
		}]],
		timed_out [[= error_info{
			.message = "request timed out",
			.retryable = true,
		}]],
		cancelled [[= error_info{
			.message = "request cancelled",
		}]],
		too_large [[= error_info{
			.message = "response exceeded the body limit",
		}]]
	};

	struct header {
		std::string name;
		std::string value;
	};

	struct request {
		http::method verb = method::get;
		std::string url;
		std::vector<header> headers;
		std::string body;
		time timeout{ seconds(15.f) };
		std::size_t max_body_bytes = 64ull * 1024ull * 1024ull;
	};

	struct response {
		std::uint16_t status = 0;
		std::vector<header> headers;
		std::string body;

		[[nodiscard]] auto header_value(
			std::string_view name
		) const -> std::string_view;

		[[nodiscard]] auto ok() const -> bool;
	};

	using result = std::expected<response, error>;

	constexpr auto method_of(
		method verb
	) -> method_info;

	constexpr auto error_of(
		error code
	) -> error_info;

	auto method_name(
		method verb
	) -> std::string;

	auto message(
		error code
	) -> std::string;

	constexpr auto retryable(
		error code
	) -> bool;
}

constexpr auto gse::http::method_of(const method verb) -> method_info {
	return annotation_from_enum<method_info>(verb, {
		.verb = "GET",
	});
}

constexpr auto gse::http::error_of(const error code) -> error_info {
	return annotation_from_enum<error_info>(code, {
		.message = "unknown error",
	});
}

auto gse::http::method_name(const method verb) -> std::string {
	const method_info info = method_of(verb);
	return info.verb;
}

auto gse::http::message(const error code) -> std::string {
	const error_info info = error_of(code);
	return info.message;
}

constexpr auto gse::http::retryable(const error code) -> bool {
	const error_info info = error_of(code);
	return info.retryable;
}

auto gse::http::response::header_value(const std::string_view name) const -> std::string_view {
	for (const auto& entry : headers) {
		if (std::ranges::equal(entry.name, name, [](const char a, const char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		})) {
			return entry.value;
		}
	}
	return {};
}

auto gse::http::response::ok() const -> bool {
	return status >= 200 && status < 300;
}
