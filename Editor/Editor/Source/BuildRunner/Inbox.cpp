module gse.ide.build:inbox_impl;

import gse;
import std;

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
		if (c == '\n' || c == '\r') {
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

auto gse::ide::build_inbox::presence_dir() -> std::filesystem::path {
	return directory() / "active";
}

auto gse::ide::build_inbox::publish_presence(const presence& active) -> void {
	std::error_code ec;
	std::filesystem::create_directories(presence_dir(), ec);

	const std::filesystem::path final_path = presence_dir() / (active.agent + ".txt");
	const std::filesystem::path staging = presence_dir() / (active.agent + ".partial");
	{
		std::ofstream out(staging, std::ios::binary | std::ios::trunc);
		if (!out) {
			return;
		}
		out << "agent " << active.agent << '\n';
		out << "name " << sanitize(active.name) << '\n';
		out << "tree " << active.tree << '\n';
	}

	std::filesystem::remove(final_path, ec);
	ec.clear();
	std::filesystem::rename(staging, final_path, ec);
}

auto gse::ide::build_inbox::clear_presence(const std::string_view agent) -> void {
	std::error_code ec;
	std::filesystem::remove(presence_dir() / (std::string(agent) + ".txt"), ec);
}

auto gse::ide::build_inbox::take_presence() -> std::vector<presence> {
	const std::filesystem::path dir = presence_dir();
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec) || ec) {
		return {};
	}

	const auto abandoned = std::chrono::seconds(30);

	std::vector<presence> out;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
		if (entry.path().extension() != ".txt") {
			continue;
		}

		std::error_code stamp_ec;
		const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(entry.path(), stamp_ec);
		if (!stamp_ec && std::filesystem::file_time_type::clock::now() - stamp > abandoned) {
			std::filesystem::remove(entry.path(), ec);
			continue;
		}

		presence parsed;
		std::ifstream in(entry.path(), std::ios::binary);
		std::string line;
		while (in && std::getline(in, line)) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			const auto [key, value] = split_field(line);
			if (key == "agent") {
				parsed.agent.assign(value);
			}
			else if (key == "name") {
				parsed.name.assign(value);
			}
			else if (key == "tree") {
				parsed.tree.assign(value);
			}
		}

		if (!parsed.agent.empty()) {
			out.push_back(std::move(parsed));
		}
	}
	return out;
}

auto gse::ide::build_inbox::peek_hibernations() -> std::vector<hibernate_request> {
	const std::filesystem::path dir = hibernate_dir();
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec) || ec) {
		return {};
	}

	const auto abandoned = std::chrono::minutes(5);

	std::vector<hibernate_request> out;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
		if (entry.path().extension() != ".txt") {
			continue;
		}

		std::error_code stamp_ec;
		const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(entry.path(), stamp_ec);
		if (!stamp_ec && std::filesystem::file_time_type::clock::now() - stamp > abandoned) {
			log::println(log::level::warning, log::category::task, "build inbox: dropping '{}' - no editor claimed it, so no project here owns that chat", entry.path());
			std::filesystem::remove(entry.path(), ec);
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
				else if (key == "cwd") {
					parsed.cwd.assign(value);
				}
			}
		}

		if (parsed.id.empty() || parsed.agent.empty() || parsed.id != entry.path().stem().generic_display_string()) {
			log::println(log::level::warning, log::category::task, "build inbox: '{}' is not a usable hibernate request", entry.path());
			std::filesystem::remove(entry.path(), ec);
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
		else if (key == "profile") {
			parsed.profile.assign(value);
		}
		else if (key == "cwd") {
			parsed.cwd.assign(value);
		}
		else if (key == "project") {
			parsed.project.assign(value);
		}
		else if (key == "run") {
			parsed.run = value == "1" || value == "true";
		}
	}

	if (parsed.id != path.stem().generic_display_string()) {
		log::println(log::level::warning, log::category::task, "build inbox: '{}' does not name itself, so there is nobody to answer", path);
		return std::nullopt;
	}
	if (parsed.target.empty()) {
		parsed.target = "game";
	}
	return parsed;
}

auto gse::ide::build_inbox::peek_requests() -> std::vector<request> {
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
	for (const auto& [_, path] : found) {
		std::optional<request> parsed = read_request(path);
		if (!parsed) {
			std::filesystem::remove(path, ec);
			continue;
		}
		out.push_back(std::move(*parsed));
	}
	return out;
}

auto gse::ide::build_inbox::consume_request(const std::string_view id) -> void {
	std::error_code ec;
	std::filesystem::remove(requests_dir() / (std::string(id) + ".txt"), ec);
}

auto gse::ide::build_inbox::consume_hibernation(const std::string_view id) -> void {
	std::error_code ec;
	std::filesystem::remove(hibernate_dir() / (std::string(id) + ".txt"), ec);
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
		out << "profile " << pending.profile << '\n';
		out << "cwd " << pending.cwd.generic_display_string() << '\n';
		out << "project " << pending.project.generic_display_string() << '\n';
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

auto gse::ide::build_inbox::queries_dir() -> std::filesystem::path {
	return directory() / "queries";
}

auto gse::ide::build_inbox::consume_symbol_query(const std::string_view id) -> void {
	std::error_code ec;
	std::filesystem::remove(queries_dir() / (std::string(id) + ".txt"), ec);
}

auto gse::ide::build_inbox::peek_symbol_queries() -> std::vector<symbol_query> {
	const std::filesystem::path dir = queries_dir();
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec) || ec) {
		return {};
	}

	const auto abandoned = std::chrono::seconds(60);

	std::vector<symbol_query> out;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
		if (entry.path().extension() != ".txt") {
			continue;
		}

		std::error_code stamp_ec;
		const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(entry.path(), stamp_ec);
		if (!stamp_ec && std::filesystem::file_time_type::clock::now() - stamp > abandoned) {
			std::filesystem::remove(entry.path(), ec);
			continue;
		}

		symbol_query parsed;
		{
			std::ifstream in(entry.path(), std::ios::binary);
			std::string line;
			while (in && std::getline(in, line)) {
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				const auto [key, value] = split_field(line);
				if (key == "id") {
					parsed.id.assign(value);
				}
				else if (key == "agent") {
					parsed.agent.assign(value);
				}
				else if (key == "name") {
					parsed.name.assign(value);
				}
				else if (key == "file") {
					parsed.file.assign(value);
				}
				else if (key == "cwd") {
					parsed.cwd.assign(value);
				}
				else if (key == "project") {
					parsed.project.assign(value);
				}
				else if (key == "sites") {
					std::from_chars(value.data(), value.data() + value.size(), parsed.sites);
				}
				else if (key == "lines") {
					std::from_chars(value.data(), value.data() + value.size(), parsed.lines);
				}
				else if (key == "body") {
					parsed.body = value == "1" || value == "true";
				}
				else if (key == "refs") {
					parsed.refs = value == "1" || value == "true";
				}
			}
		}

		if (parsed.id.empty() || parsed.id != entry.path().stem().generic_display_string()) {
			log::println(log::level::warning, log::category::task, "build inbox: '{}' is not a usable symbol query", entry.path());
			std::filesystem::remove(entry.path(), ec);
			continue;
		}
		if (parsed.name.empty() && parsed.file.empty()) {
			log::println(log::level::warning, log::category::task, "build inbox: symbol query '{}' names neither a symbol nor a file", entry.path());
			std::filesystem::remove(entry.path(), ec);
			continue;
		}
		out.push_back(std::move(parsed));
	}
	return out;
}
