module gse.ide.agent:stream_impl;

import std;
import gse;

import gse.ide.analysis;

import :model;
import :stream;

auto gse::ide::agent::escape_json(const std::string_view text) -> std::string {
	std::string out;
	out.reserve(text.size() + 16);
	for (const char ch : text) {
		switch (ch) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(ch) < 0x20) {
					out += std::format("\\u{:04x}", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
				}
				else {
					out.push_back(ch);
				}
				break;
		}
	}
	return out;
}

auto gse::ide::agent::encode_base64(const std::span<const std::byte> bytes) -> std::string {
	constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string out;
	out.reserve((bytes.size() + 2) / 3 * 4);

	for (std::size_t i = 0; i < bytes.size(); i += 3) {
		const std::size_t remaining = bytes.size() - i;
		const std::uint32_t a = static_cast<std::uint32_t>(bytes[i]);
		const std::uint32_t b = remaining > 1 ? static_cast<std::uint32_t>(bytes[i + 1]) : 0u;
		const std::uint32_t c = remaining > 2 ? static_cast<std::uint32_t>(bytes[i + 2]) : 0u;
		const std::uint32_t triple = a << 16 | b << 8 | c;

		out += alphabet[triple >> 18 & 0x3f];
		out += alphabet[triple >> 12 & 0x3f];
		out += remaining > 1 ? alphabet[triple >> 6 & 0x3f] : '=';
		out += remaining > 2 ? alphabet[triple & 0x3f] : '=';
	}

	return out;
}

auto gse::ide::agent::read_file_bytes(const std::filesystem::path& path) -> std::vector<std::byte> {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		return {};
	}

	const std::streamoff size = file.tellg();
	if (size <= 0) {
		return {};
	}

	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(bytes.data()), size);
	if (!file) {
		return {};
	}

	return bytes;
}

auto gse::ide::agent::lowered_extension(const std::filesystem::path& path) -> std::string {
	std::string extension = path.extension().display_string();
	std::ranges::transform(extension, extension.begin(), [](const char ch) {
		return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	});
	return extension;
}

auto gse::ide::agent::sendable_encoding(const std::filesystem::path& path) -> bool {
	const std::string extension = lowered_extension(path);
	return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".gif" || extension == ".webp";
}

auto gse::ide::agent::media_type_for(const std::filesystem::path& path) -> std::string_view {
	const std::string extension = lowered_extension(path);
	if (extension == ".jpg" || extension == ".jpeg") {
		return "image/jpeg";
	}
	if (extension == ".gif") {
		return "image/gif";
	}
	if (extension == ".webp") {
		return "image/webp";
	}
	return "image/png";
}

auto gse::ide::agent::user_message(const std::string_view prompt, const std::span<const attachment> attachments) -> std::string {
	std::string out = R"({"type":"user","message":{"role":"user","content":[)";

	for (const attachment& a : attachments) {
		const std::string encoded = encode_base64(read_file_bytes(a.path));
		if (encoded.empty()) {
			continue;
		}
		out += R"({"type":"image","source":{"type":"base64","media_type":")";
		out += media_type_for(a.path);
		out += R"(","data":")";
		out += encoded;
		out += R"("}},)";
	}

	out += R"({"type":"text","text":")";
	out += escape_json(prompt);
	out += R"("}]}})";
	out += '\n';
	return out;
}

auto gse::ide::agent::string_at(const analysis::json::value& parent, const std::string_view key) -> std::string_view {
	const analysis::json::value* found = parent.find(key);
	return found ? found->as_string() : std::string_view{};
}

auto gse::ide::agent::collect_denials(const analysis::json::value& event, std::vector<transcript_row>& out) -> void {
	const analysis::json::value* denials = event.find("permission_denials");
	if (!denials || !denials->is_array()) {
		return;
	}
	for (const analysis::json::value& denial : denials->children) {
		const std::string_view tool = string_at(denial, "tool_name");
		out.push_back({
			.kind = row_kind::denial,
			.text = std::format("denied: {}", tool.empty() ? std::string_view("tool") : tool),
			.detail = std::string(string_at(denial, "message")),
		});
	}
}

auto gse::ide::agent::to_lines(const std::string_view text) -> std::vector<std::string> {
	std::vector<std::string> lines;
	std::size_t start = 0;
	while (start <= text.size()) {
		const std::size_t newline = text.find('\n', start);
		const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
		std::string_view line = text.substr(start, end - start);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}
		lines.emplace_back(line);
		if (newline == std::string_view::npos) {
			break;
		}
		start = newline + 1;
	}
	return lines;
}

auto gse::ide::agent::tool_row(const analysis::json::value& block) -> transcript_row {
	const std::string_view name = string_at(block, "name");
	transcript_row row = {
		.kind = row_kind::tool,
		.text = std::string(name),
	};

	const analysis::json::value* input = block.find("input");
	if (!input) {
		return row;
	}

	const std::string_view path = string_at(*input, "file_path");
	if (!path.empty()) {
		row.file = std::filesystem::path(path);
		row.detail = row.file.filename().generic_display_string();
	}

	if (name == "Edit") {
		row.removed = to_lines(string_at(*input, "old_string"));
		row.added = to_lines(string_at(*input, "new_string"));
		return row;
	}

	if (name == "Write") {
		row.added = to_lines(string_at(*input, "content"));
		return row;
	}

	if (name == "Bash" || name == "PowerShell") {
		row.detail = std::string(string_at(*input, "command"));
		return row;
	}

	return row;
}

auto gse::ide::agent::jump_line_for(const transcript_row& row) -> std::uint32_t {
	return row.start_line ? *row.start_line - 1 : 0;
}

auto gse::ide::agent::int_at(const analysis::json::value& parent, const std::string_view key) -> std::int64_t {
	const analysis::json::value* found = parent.find(key);
	return found ? found->as_int() : 0;
}

auto gse::ide::agent::record_usage(const analysis::json::value& message, session_info& info) -> void {
	const analysis::json::value* usage = message.find("usage");
	if (!usage) {
		return;
	}

	const std::int64_t prefix = int_at(*usage, "input_tokens")
		+ int_at(*usage, "cache_creation_input_tokens")
		+ int_at(*usage, "cache_read_input_tokens");
	if (prefix <= 0) {
		return;
	}

	info.context_used = prefix + int_at(*usage, "output_tokens");
	if (info.context_base == 0) {
		info.context_base = prefix;
	}
}

auto gse::ide::agent::summarize(const analysis::json::value& event, session_info& info) -> std::vector<transcript_row> {
	std::vector<transcript_row> out;
	const std::string_view kind = string_at(event, "type");

	if (kind == "system") {
		const std::string_view subtype = string_at(event, "subtype");
		if (subtype == "thinking_tokens" || subtype == "post_turn_summary" || subtype == "hook_started") {
			return out;
		}
		if (subtype == "hook_response") {
			const analysis::json::value* code = event.find("exit_code");
			if (code && code->as_int() != 0) {
				out.push_back({
					.kind = row_kind::failure,
					.text = std::format("hook {} failed", string_at(event, "hook_name")),
					.detail = std::string(string_at(event, "stderr")),
				});
			}
			return out;
		}
		if (subtype == "init") {
			info.agent_id = std::string(string_at(event, "session_id"));
			info.model = std::string(string_at(event, "model"));
			return out;
		}
		if (subtype == "api_retry") {
			const analysis::json::value* status = event.find("error_status");
			out.push_back({
				.kind = row_kind::failure,
				.text = std::format("retry {}", status ? status->as_int() : 0),
				.detail = std::string(string_at(event, "error")),
			});
			return out;
		}
		out.push_back({
			.kind = row_kind::note,
			.text = std::format("[{}]", subtype.empty() ? kind : subtype),
		});
		return out;
	}

	if (kind == "rate_limit_event") {
		const analysis::json::value* info = event.find("rate_limit_info");
		if (!info) {
			return out;
		}
		const analysis::json::value* overage = info->find("isUsingOverage");
		const std::string_view status = string_at(*info, "status");
		if (status == "allowed" && !(overage && overage->boolean)) {
			return out;
		}
		out.push_back({
			.kind = row_kind::failure,
			.text = std::format("{} limit: {}{}", string_at(*info, "rateLimitType"), status, overage && overage->boolean ? " (overage)" : ""),
		});
		return out;
	}

	if (kind == "control_request") {
		const analysis::json::value* request = event.find("request");
		out.push_back({
			.kind = row_kind::note,
			.text = std::format("control request: {}", request ? string_at(*request, "subtype") : std::string_view{}),
		});
		return out;
	}

	if (kind == "user") {
		return out;
	}

	if (kind == "assistant") {
		const analysis::json::value* message = event.find("message");
		if (message) {
			record_usage(*message, info);
		}

		const analysis::json::value* content = message ? message->find("content") : nullptr;
		if (!content || !content->is_array()) {
			return out;
		}
		for (const analysis::json::value& block : content->children) {
			const std::string_view block_kind = string_at(block, "type");
			if (block_kind == "text") {
				out.push_back({
					.kind = row_kind::text,
					.text = std::string(string_at(block, "text")),
				});
			}
			else if (block_kind == "tool_use") {
				out.push_back(tool_row(block));
			}
		}
		return out;
	}

	if (kind == "result") {
		collect_denials(event, out);

		const analysis::json::value* error = event.find("is_error");
		if (error && error->boolean) {
			const analysis::json::value* status = event.find("api_error_status");
			const std::string_view result = string_at(event, "result");
			std::string headline = status && status->as_int() == 401
				? "authentication failed - set CLAUDE_CODE_OAUTH_TOKEN"
				: status
				? std::format("error {}", status->as_int())
				: result.empty()
				? "the agent failed"
				: std::string(result.substr(0, result.find('\n')));
			info.failure = headline;
			out.push_back({
				.kind = row_kind::failure,
				.text = std::move(headline),
				.detail = std::string(result),
			});
			return out;
		}

		info.failure.clear();

		const analysis::json::value* turns = event.find("num_turns");
		const analysis::json::value* duration = event.find("duration_api_ms");
		const analysis::json::value* cost = event.find("total_cost_usd");
		if (turns) {
			info.turns = turns->as_int();
		}
		if (duration) {
			info.api_time += milliseconds(static_cast<float>(duration->as_int()));
		}
		if (cost) {
			info.cost += cost->number;
		}
		return out;
	}

	out.push_back({
		.kind = row_kind::note,
		.text = std::format("[{}]", kind),
	});
	return out;
}

auto gse::ide::agent::turn_failed(const analysis::json::value& event) -> bool {
	const analysis::json::value* error = event.find("is_error");
	return error && error->boolean;
}

auto gse::ide::agent::retryable_failure(const analysis::json::value& event) -> bool {
	if (!turn_failed(event)) {
		return false;
	}

	if (const analysis::json::value* status = event.find("api_error_status")) {
		const std::int64_t code = status->as_int();
		return code == 408 || code == 429 || code >= 500;
	}

	static constexpr std::string_view transport[] = {
		"api error",
		"connection error",
		"unable to connect",
		"fetch failed",
		"network",
		"socket hang up",
		"getaddrinfo",
		"enotfound",
		"econnreset",
		"econnrefused",
		"etimedout",
		"eai_again",
		"timed out",
		"offline",
	};

	std::string message(string_at(event, "result"));
	std::ranges::transform(message, message.begin(), [](const unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return std::ranges::any_of(transport, [&message](const std::string_view mark) {
		return message.contains(mark);
	});
}

auto gse::ide::agent::anchorable(const analysis::json::value& event) -> bool {
	if (string_at(event, "type") != "assistant") {
		return false;
	}

	const analysis::json::value* message = event.find("message");
	const analysis::json::value* content = message ? message->find("content") : nullptr;
	if (!content || !content->is_array()) {
		return false;
	}

	return std::ranges::none_of(content->children, [](const analysis::json::value& block) {
		return string_at(block, "type") == "tool_use";
	});
}

auto gse::ide::agent::user_rows(const analysis::json::value& event) -> std::vector<transcript_row> {
	std::vector<transcript_row> out;

	const analysis::json::value* message = event.find("message");
	const analysis::json::value* content = message ? message->find("content") : nullptr;
	if (!content || !content->is_array()) {
		return out;
	}

	for (const analysis::json::value& block : content->children) {
		if (string_at(block, "type") != "text") {
			continue;
		}
		std::string text(string_at(block, "text"));
		if (text.empty()) {
			continue;
		}
		out.push_back({
			.kind = row_kind::user,
			.text = std::move(text),
		});
	}

	return out;
}

auto gse::ide::agent::tool_action(const transcript_row& row) -> std::string {
	constexpr std::size_t subject_limit = 48;

	std::string_view subject = row.detail;
	if (const std::size_t newline = subject.find('\n'); newline != std::string_view::npos) {
		subject = subject.substr(0, newline);
	}
	if (subject.empty()) {
		return row.text;
	}
	return subject.size() > subject_limit
		? std::format("{} {}...", row.text, subject.substr(0, subject_limit))
		: std::format("{} {}", row.text, subject);
}
