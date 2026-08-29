export module gse.http:url;

import std;

export namespace gse::http {
	struct url {
		std::string scheme;
		std::string host;
		std::uint16_t port = 0;
		std::string target;
		bool secure = false;
	};

	struct query_parameter {
		std::string name;
		std::string value;
	};

	auto parse_url(
		std::string_view text
	) -> std::optional<url>;

	auto percent_encode(
		std::string_view text
	) -> std::string;

	auto build_query(
		std::span<const query_parameter> parameters
	) -> std::string;

	auto with_query(
		std::string_view base,
		std::span<const query_parameter> parameters
	) -> std::string;

	auto widen(
		std::string_view text
	) -> std::wstring;

	auto narrow(
		std::wstring_view text
	) -> std::string;
}

auto gse::http::parse_url(const std::string_view text) -> std::optional<url> {
	const auto scheme_end = text.find("://");
	if (scheme_end == std::string_view::npos || scheme_end == 0) {
		return std::nullopt;
	}

	url out;
	out.scheme.reserve(scheme_end);
	for (const char c : text.substr(0, scheme_end)) {
		out.scheme.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}

	if (out.scheme == "https") {
		out.secure = true;
		out.port = 443;
	}
	else if (out.scheme == "http") {
		out.port = 80;
	}
	else {
		return std::nullopt;
	}

	const std::string_view remainder = text.substr(scheme_end + 3);
	const auto authority_end = remainder.find_first_of("/?#");
	const std::string_view authority = authority_end == std::string_view::npos ? remainder : remainder.substr(0, authority_end);

	if (authority.empty()) {
		return std::nullopt;
	}

	const auto credentials_end = authority.rfind('@');
	const std::string_view host_port = credentials_end == std::string_view::npos ? authority : authority.substr(credentials_end + 1);

	const auto port_start = host_port.rfind(':');
	if (port_start != std::string_view::npos && host_port.find(']') == std::string_view::npos) {
		const std::string_view digits = host_port.substr(port_start + 1);
		std::uint32_t parsed = 0;
		const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), parsed);
		if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size() || parsed == 0 || parsed > 65535) {
			return std::nullopt;
		}
		out.port = static_cast<std::uint16_t>(parsed);
		out.host.assign(host_port.substr(0, port_start));
	}
	else {
		out.host.assign(host_port);
	}

	if (out.host.empty()) {
		return std::nullopt;
	}

	if (authority_end == std::string_view::npos) {
		out.target = "/";
		return out;
	}

	const std::string_view tail = remainder.substr(authority_end);
	const auto fragment = tail.find('#');
	out.target.assign(fragment == std::string_view::npos ? tail : tail.substr(0, fragment));
	if (out.target.empty() || out.target.front() != '/') {
		out.target.insert(out.target.begin(), '/');
	}
	return out;
}

auto gse::http::percent_encode(const std::string_view text) -> std::string {
	constexpr std::string_view unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

	std::string out;
	out.reserve(text.size());
	for (const char c : text) {
		if (unreserved.find(c) != std::string_view::npos) {
			out.push_back(c);
		}
		else {
			out.append(std::format("%{:02X}", static_cast<unsigned>(static_cast<unsigned char>(c))));
		}
	}
	return out;
}

auto gse::http::build_query(const std::span<const query_parameter> parameters) -> std::string {
	std::string out;
	for (const auto& [name, value] : parameters) {
		if (!out.empty()) {
			out.push_back('&');
		}
		out.append(percent_encode(name));
		out.push_back('=');
		out.append(percent_encode(value));
	}
	return out;
}

auto gse::http::with_query(const std::string_view base, const std::span<const query_parameter> parameters) -> std::string {
	const std::string query = build_query(parameters);
	if (query.empty()) {
		return std::string(base);
	}

	std::string out(base);
	out.push_back(out.find('?') == std::string::npos ? '?' : '&');
	out.append(query);
	return out;
}

auto gse::http::widen(const std::string_view text) -> std::wstring {
	std::wstring out;
	out.reserve(text.size());

	std::size_t i = 0;
	while (i < text.size()) {
		const auto lead = static_cast<unsigned char>(text[i]);
		std::uint32_t code_point = 0;
		std::size_t extra = 0;

		if (lead < 0x80u) {
			code_point = lead;
		}
		else if ((lead & 0xe0u) == 0xc0u) {
			code_point = lead & 0x1fu;
			extra = 1;
		}
		else if ((lead & 0xf0u) == 0xe0u) {
			code_point = lead & 0x0fu;
			extra = 2;
		}
		else if ((lead & 0xf8u) == 0xf0u) {
			code_point = lead & 0x07u;
			extra = 3;
		}
		else {
			code_point = 0xfffdu;
		}

		if (i + extra >= text.size()) {
			code_point = 0xfffdu;
			extra = 0;
		}

		for (std::size_t n = 1; n <= extra; ++n) {
			const auto continuation = static_cast<unsigned char>(text[i + n]);
			if ((continuation & 0xc0u) != 0x80u) {
				code_point = 0xfffdu;
				extra = 0;
				break;
			}
			code_point = (code_point << 6) | (continuation & 0x3fu);
		}

		i += extra + 1;

		if (code_point <= 0xffffu) {
			out.push_back(static_cast<wchar_t>(code_point));
		}
		else {
			const std::uint32_t adjusted = code_point - 0x10000u;
			out.push_back(static_cast<wchar_t>(0xd800u + (adjusted >> 10)));
			out.push_back(static_cast<wchar_t>(0xdc00u + (adjusted & 0x3ffu)));
		}
	}

	return out;
}

auto gse::http::narrow(const std::wstring_view text) -> std::string {
	std::string out;
	out.reserve(text.size());

	std::size_t i = 0;
	while (i < text.size()) {
		auto code_point = static_cast<std::uint32_t>(static_cast<std::uint16_t>(text[i]));
		++i;

		if (code_point >= 0xd800u && code_point <= 0xdbffu && i < text.size()) {
			const auto low = static_cast<std::uint32_t>(static_cast<std::uint16_t>(text[i]));
			if (low >= 0xdc00u && low <= 0xdfffu) {
				code_point = 0x10000u + ((code_point - 0xd800u) << 10) + (low - 0xdc00u);
				++i;
			}
		}

		if (code_point <= 0x7fu) {
			out.push_back(static_cast<char>(code_point));
		}
		else if (code_point <= 0x7ffu) {
			out.push_back(static_cast<char>(0xc0u | (code_point >> 6)));
			out.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
		}
		else if (code_point <= 0xffffu) {
			out.push_back(static_cast<char>(0xe0u | (code_point >> 12)));
			out.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3fu)));
			out.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
		}
		else {
			out.push_back(static_cast<char>(0xf0u | (code_point >> 18)));
			out.push_back(static_cast<char>(0x80u | ((code_point >> 12) & 0x3fu)));
			out.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3fu)));
			out.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
		}
	}

	return out;
}
