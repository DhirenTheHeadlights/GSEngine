module gse.ide.build:inbox_impl;

import std;
import gse;

import :inbox;

namespace gse::ide::build_inbox {
	auto split_field(
		std::string_view line
	) -> std::pair<std::string_view, std::string_view>;

	auto sanitize(
		std::string_view text
	) -> std::string;

	auto read_request(
		const std::filesystem::path& path
	) -> std::optional<request>;
}

auto gse::ide::build_inbox::split_field(const std::string_view line) -> std::pair<std::string_view, std::string_view> {
	const std::size_t space = line.find(' ');
	if (space == std::string_view::npos) {
		return { line, {} };
	}
	std::string_view value = line.substr(space + 1);
	while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
		value.remove_prefix(1);
	}
	return { line.substr(0, space), value };
}

auto gse::ide::build_inbox::sanitize(const std::string_view text) -> std::string {
	std::string out(text);
	for (char& c : out) {
		if (c == '\n' || c == '\r' || c == '\t') {
			c = ' ';
		}
	}
	return out;
}

auto gse::ide::build_inbox::directory() -> std::filesystem::path {
	return config::cache_dir() / "agent-build";
}

auto gse::ide::build_inbox::requests_dir() -> std::filesystem::path {
	return directory() / "requests";
}

auto gse::ide::build_inbox::results_dir() -> std::filesystem::path {
	return directory() / "results";
}

auto gse::ide::build_inbox::hibernate_dir() -> std::filesystem::path {
	return directory() / "hibernate";
}

auto gse::ide::build_inbox::take_hibernations() -> std::vector<hibernate_request> {
	const std::filesystem::path dir = hibernate_dir();
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec) || ec) {
		return {};
	}

	std::vector<hibernate_request> out;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
		if (entry.path().extension() != ".txt") {
			continue;
		}

		hibernate_request parsed;
		bool in_prompt = false;
		{
			std::ifstream in(entry.path(), std::ios::binary);
			std::string line;
			while (in && std::getline(in, line)) {
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				if (in_prompt) {
					if (!parsed.prompt.empty()) {
						parsed.prompt += '\n';
					}
					parsed.prompt += line;
					continue;
				}
				if (line == "prompt") {
					in_prompt = true;
					continue;
				}
				const auto [key, value] = split_field(line);
				if (key == "id") {
					parsed.id.assign(value);
				}
				else if (key == "agent") {
					parsed.agent.assign(value);
				}
			}
		}
		std::filesystem::remove(entry.path(), ec);

		if (parsed.id.empty() || parsed.agent.empty()) {
			log::println(log::level::warning, log::category::task, "build inbox: '{}' is not a usable hibernate request", entry.path());
			continue;
		}
		out.push_back(std::move(parsed));
	}
	return out;
}

auto gse::ide::build_inbox::read_request(const std::filesystem::path& path) -> std::optional<request> {
	std::vector<std::string> lines;
	{
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			return std::nullopt;
		}
		std::string line;
		while (std::getline(in, line)) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			if (!line.empty()) {
				lines.push_back(std::move(line));
			}
		}
	}

	request parsed;
	for (const std::string& line : lines) {
		const auto [key, value] = split_field(line);
		if (key == "id") {
			parsed.id.assign(value);
		}
		else if (key == "agent") {
			parsed.agent.assign(value);
		}
		else if (key == "target") {
			parsed.target.assign(value);
		}
		else if (key == "tree") {
			parsed.tree.assign(value);
		}
		else if (key == "run") {
			parsed.run = value == "1" || value == "true";
		}
	}

	if (parsed.id.empty()) {
		log::println(log::level::warning, log::category::task, "build inbox: '{}' has no id field, so there is nobody to answer", path);
		return std::nullopt;
	}
	if (parsed.target.empty()) {
		parsed.target = "game";
	}
	return parsed;
}

auto gse::ide::build_inbox::take_requests() -> std::vector<request> {
	const std::filesystem::path dir = requests_dir();
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec) || ec) {
		return {};
	}

	std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> found;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
		if (entry.path().extension() != ".txt") {
			continue;
		}
		std::error_code stamp_ec;
		found.emplace_back(std::filesystem::last_write_time(entry.path(), stamp_ec), entry.path());
	}
	std::ranges::sort(found, {}, &std::pair<std::filesystem::file_time_type, std::filesystem::path>::first);

	std::vector<request> out;
	for (const auto& [stamp, path] : found) {
		std::optional<request> parsed = read_request(path);
		std::filesystem::remove(path, ec);
		if (parsed) {
			out.push_back(std::move(*parsed));
		}
	}
	return out;
}

auto gse::ide::build_inbox::restore(const request& pending) -> void {
	std::error_code ec;
	std::filesystem::create_directories(requests_dir(), ec);

	const std::filesystem::path final_path = requests_dir() / (pending.id + ".txt");
	const std::filesystem::path staging = requests_dir() / (pending.id + ".partial");
	{
		std::ofstream out(staging, std::ios::binary | std::ios::trunc);
		if (!out) {
			log::println(log::level::warning, log::category::task, "build inbox: could not hand '{}' to the next editor", pending.id);
			return;
		}
		out << "id " << pending.id << '\n';
		out << "agent " << pending.agent << '\n';
		out << "target " << pending.target << '\n';
		out << "run " << (pending.run ? '1' : '0') << '\n';
		out << "tree " << pending.tree << '\n';
	}

	std::filesystem::rename(staging, final_path, ec);
	if (ec) {
		log::println(log::level::warning, log::category::task, "build inbox: could not hand '{}' to the next editor ({})", pending.id, ec.message());
	}
}

auto gse::ide::build_inbox::withdraw(const std::string_view id) -> void {
	std::error_code ec;
	std::filesystem::remove(results_dir() / (std::string(id) + ".txt"), ec);
}

auto gse::ide::build_inbox::publish(const result& outcome) -> void {
	std::error_code ec;
	std::filesystem::create_directories(results_dir(), ec);

	const std::filesystem::path final_path = results_dir() / (outcome.id + ".txt");
	const std::filesystem::path staging = results_dir() / (outcome.id + ".partial");
	{
		std::ofstream out(staging, std::ios::binary | std::ios::trunc);
		if (!out) {
			log::println(log::level::warning, log::category::task, "build inbox: could not open '{}' for writing", staging);
			return;
		}
		out << "id " << outcome.id << '\n';
		out << "status " << enum_to_string(outcome.outcome) << '\n';
		out << "owned " << outcome.owned << '\n';
		for (const std::string& line : outcome.lines) {
			out << sanitize(line) << '\n';
		}
	}

	std::filesystem::remove(final_path, ec);
	ec.clear();
	std::filesystem::rename(staging, final_path, ec);
	if (ec) {
		log::println(log::level::warning, log::category::task, "build inbox: could not publish '{}' ({})", final_path, ec.message());
	}
}
