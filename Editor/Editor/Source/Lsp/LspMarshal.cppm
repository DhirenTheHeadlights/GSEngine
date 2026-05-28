export module ide:lsp_marshal;

import std;
import gse;

import :lsp_protocol;

export namespace ide::lsp {
	template <typename T>
	auto to_json(const T& value) -> std::string;

	template <typename T>
	auto from_json(std::string_view text, T& out) -> bool;

	template <typename T>
	auto request_envelope(std::int32_t id, const T& body) -> std::string;

	template <typename T>
	auto notification_envelope(const T& body) -> std::string;
}

template <typename T>
auto ide::lsp::to_json(const T& value) -> std::string {
	(void)value;
	return "{}";
}

template <typename T>
auto ide::lsp::from_json(std::string_view text, T& out) -> bool {
	(void)text;
	(void)out;
	return false;
}

template <typename T>
auto ide::lsp::request_envelope(std::int32_t id, const T& body) -> std::string {
	return std::format(
		R"({{"jsonrpc":"2.0","id":{},"method":"{}","params":{}}})",
		id,
		T::method,
		to_json(body)
	);
}

template <typename T>
auto ide::lsp::notification_envelope(const T& body) -> std::string {
	return std::format(
		R"({{"jsonrpc":"2.0","method":"{}","params":{}}})",
		T::method,
		to_json(body)
	);
}
