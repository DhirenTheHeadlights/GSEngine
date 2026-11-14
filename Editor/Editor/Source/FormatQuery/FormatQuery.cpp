module gse.ide.format_query;

import gse;
import gse.format;
import gse.ide.app;
import gse.ide.build;
import gse.ide.config;
import gse.ide.workspace;
import std;

auto gse::ide::read_source(const std::filesystem::path& path) -> std::optional<source_text> {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return std::nullopt;
	}
	const std::string content{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };

	source_text out;
	out.crlf = content.contains("\r\n");
	for (auto part : content | std::views::split('\n')) {
		std::string line(part.begin(), part.end());
		if (out.crlf && line.ends_with('\r')) {
			line.pop_back();
		}
		out.lines.push_back(std::move(line));
	}
	return out;
}

auto gse::ide::write_source(const std::filesystem::path& path, const source_text& text) -> bool {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		return false;
	}
	const std::string_view newline = text.crlf ? "\r\n" : "\n";
	for (std::size_t i = 0; i < text.lines.size(); ++i) {
		if (i > 0) {
			file << newline;
		}
		file << text.lines[i];
	}
	return static_cast<bool>(file);
}

auto gse::ide::open_and_unsaved(const workspace::data& ws, const std::filesystem::path& path) -> bool {
	for (const id doc_id : ws.documents.order()) {
		const document& doc = ws.documents.at(doc_id);
		std::error_code ec;
		if (doc.persistence != document_persistence::clean && std::filesystem::equivalent(doc.path, path, ec)) {
			return true;
		}
	}
	return false;
}

auto gse::ide::answer_format_request(const workspace::data& ws, const format::options& opts, const build_inbox::format_request& request) -> std::vector<std::string> {
	std::vector<std::string> out;
	const std::filesystem::path path = request.file.is_absolute() ? request.file : request.cwd / request.file;
	out.push_back(std::format("field file {}", path.generic_display_string()));

	if (open_and_unsaved(ws, path)) {
		out.push_back("error the file is open in a code panel with unsaved changes; save or close it first, because formatting the disk copy would fork it from the buffer.");
		return out;
	}

	std::optional<source_text> text = read_source(path);
	if (!text) {
		out.push_back("error the file could not be read.");
		return out;
	}

	const format::format_result first = format::format_lines(text->lines, opts);
	std::vector<std::string> again = text->lines;
	const format::format_result second = format::format_lines(again, opts);

	out.push_back(std::format("count blocks {}", first.blocks.size()));
	out.push_back(std::format("count reindented {}", first.reindented));
	out.push_back(std::format("count second_blocks {}", second.blocks.size()));
	out.push_back(std::format("count second_reindented {}", second.reindented));
	out.push_back(std::format("field idempotent {}", second.blocks.empty() && second.reindented == 0 ? "1" : "0"));

	const bool changed = !first.blocks.empty() || first.reindented > 0;
	if (request.dry_run || !changed) {
		out.push_back(std::format("field outcome {}", changed ? "would_change" : "unchanged"));
		return out;
	}
	if (!write_source(path, *text)) {
		out.push_back("error the formatted text could not be written back.");
		return out;
	}
	out.push_back("field outcome formatted");
	return out;
}

auto gse::ide::format_query_system::run(context&, data& d, const shared_view<workspace_system::data> workspace_d, const shared_view<config_system::data> config_d) -> async::task<> {
	if (!d.poll.tick()) {
		co_return;
	}

	const std::vector<build_inbox::format_request> requests = build_inbox::peek_format_requests();
	if (requests.empty()) {
		co_return;
	}

	const format::options opts{
		.indent_width = config_d.indent_width,
		.indent_with_spaces = config_d.indent_with_spaces,
	};

	for (const build_inbox::format_request& request : requests) {
		if (!config::owns_request(request.cwd, request.project)) {
			continue;
		}
		build_inbox::consume_format_request(request.id);
		build_inbox::publish({
			.id = request.id,
			.outcome = build_inbox::status::ok,
			.lines = answer_format_request(workspace_d.ws, opts, request),
		});
	}
	co_return;
}
