export module gse.json:write;

import std;

import :value;

export namespace gse::json {
	struct write_options {
		std::uint32_t indent = 0;
		bool sort_keys = false;
	};

	auto write(
		const value& root
	) -> std::string;

	auto write(
		const value& root,
		const write_options& options
	) -> std::string;

	auto escape(
		std::string_view text
	) -> std::string;
}

namespace gse::json {
	auto write_scalar(
		const value& node,
		std::string& out
	) -> void;

	auto write_node(
		const value& node,
		const write_options& options,
		std::uint32_t depth,
		std::string& out
	) -> void;

	auto write_newline(
		const write_options& options,
		std::uint32_t depth,
		std::string& out
	) -> void;

	auto member_order(
		const value& node,
		const write_options& options
	) -> std::vector<std::size_t>;
}

auto gse::json::escape(const std::string_view text) -> std::string {
	std::string out;
	out.reserve(text.size() + 2);
	for (const char c : text) {
		switch (c) {
			case '"':
				out.append("\\\"");
				break;
			case '\\':
				out.append("\\\\");
				break;
			case '\b':
				out.append("\\b");
				break;
			case '\f':
				out.append("\\f");
				break;
			case '\n':
				out.append("\\n");
				break;
			case '\r':
				out.append("\\r");
				break;
			case '\t':
				out.append("\\t");
				break;
			default:
				if (static_cast<unsigned char>(c) < 0x20u) {
					out.append(std::format("\\u{:04x}", static_cast<unsigned>(static_cast<unsigned char>(c))));
				}
				else {
					out.push_back(c);
				}
				break;
		}
	}
	return out;
}

auto gse::json::write_scalar(const value& node, std::string& out) -> void {
	switch (node.type()) {
		case value::kind::null:
			out.append("null");
			break;
		case value::kind::boolean:
			out.append(node.boolean() ? "true" : "false");
			break;
		case value::kind::number:
			if (node.is_integer()) {
				out.append(std::format("{}", node.integer()));
			}
			else {
				const double n = node.number();
				if (!std::isfinite(n)) {
					out.append("null");
				}
				else {
					out.append(std::format("{}", n));
				}
			}
			break;
		case value::kind::string:
			out.push_back('"');
			out.append(escape(node.text()));
			out.push_back('"');
			break;
		default:
			break;
	}
}

auto gse::json::write_newline(const write_options& options, const std::uint32_t depth, std::string& out) -> void {
	if (options.indent == 0) {
		return;
	}
	out.push_back('\n');
	out.append(static_cast<std::size_t>(options.indent) * depth, ' ');
}

auto gse::json::member_order(const value& node, const write_options& options) -> std::vector<std::size_t> {
	std::vector<std::size_t> order(node.size());
	std::iota(order.begin(), order.end(), std::size_t{ 0 });
	if (options.sort_keys) {
		const auto keys = node.keys();
		std::ranges::sort(order, [keys](const std::size_t a, const std::size_t b) {
			return keys[a] < keys[b];
		});
	}
	return order;
}

auto gse::json::write_node(const value& node, const write_options& options, const std::uint32_t depth, std::string& out) -> void {
	if (node.is_array()) {
		if (node.empty()) {
			out.append("[]");
			return;
		}
		out.push_back('[');
		bool first = true;
		for (const value& element : node.elements()) {
			if (!first) {
				out.push_back(',');
			}
			first = false;
			write_newline(options, depth + 1, out);
			write_node(element, options, depth + 1, out);
		}
		write_newline(options, depth, out);
		out.push_back(']');
		return;
	}

	if (node.is_object()) {
		if (node.empty()) {
			out.append("{}");
			return;
		}
		out.push_back('{');
		const auto keys = node.keys();
		const auto values = node.elements();
		bool first = true;
		for (const std::size_t index : member_order(node, options)) {
			if (!first) {
				out.push_back(',');
			}
			first = false;
			write_newline(options, depth + 1, out);
			out.push_back('"');
			out.append(escape(keys[index]));
			out.append(options.indent == 0 ? "\":" : "\": ");
			write_node(values[index], options, depth + 1, out);
		}
		write_newline(options, depth, out);
		out.push_back('}');
		return;
	}

	write_scalar(node, out);
}

auto gse::json::write(const value& root) -> std::string {
	return write(root, {});
}

auto gse::json::write(const value& root, const write_options& options) -> std::string {
	std::string out;
	write_node(root, options, 0, out);
	return out;
}
