module gse.ide.agent:draft_impl;

import gse;
import gse.ide.build;
import gse.ide.git;
import gse.win32;
import std;

import :draft;
import :model;
import :session;

auto gse::ide::agent::draft_instructions() -> std::string_view {
	return
		"Write a git commit message for the change described below.\n"
		"Reply with the commit message only: no preamble, no sign-off, no markdown fences.\n"
		"\n"
		"Shape:\n"
		"- First line: a subject under 72 characters in the imperative mood, styled like the recent subjects listed below, which are usually 'area: what changed' in lower case.\n"
		"- Then one blank line.\n"
		"- Then a detailed body of '- ' bullets grouped by subsystem. Each bullet names the files, types or functions it covers and says what the change does and why it was needed.\n"
		"- Call out behaviour changes, new state, new settings and anything load-bearing a reviewer would otherwise miss. Do not restate the diff line by line and do not pad with filler.\n"
		"\n"
		"Rules:\n"
		"- Describe only what the file list and diff below actually show.\n"
		"- Never mention AI, assistants, Claude, Anthropic, or any tool that helped write this.\n"
		"- Never add trailers: no Co-Authored-By line, no 'Generated with' line, no emoji footer.\n"
		"\n";
}

auto gse::ide::agent::diff_steps(const draft_request& request) -> std::vector<std::string> {
	constexpr std::string_view opening = "git --no-optional-locks diff HEAD --";
	std::vector<std::string> steps;
	std::string step(opening);
	for (const draft_file& file : request.files) {
		const std::string quoted = std::format(" \"{}\"", file.relative.generic_display_string());
		if (step.size() > opening.size() && step.size() + quoted.size() > draft_step_budget) {
			steps.push_back(std::exchange(step, std::string(opening)));
		}
		step += quoted;
	}
	if (step.size() > opening.size()) {
		steps.push_back(std::move(step));
	}
	return steps;
}

auto gse::ide::agent::selected_diff(const draft_request& request) -> std::expected<std::string, std::string> {
	std::string diff;
	for (const std::string& step : diff_steps(request)) {
		const std::expected<std::string, std::string> captured = git::capture(step, request.root);
		if (!captured) {
			return std::unexpected(captured.error());
		}
		diff += *captured;
	}
	return diff;
}

auto gse::ide::agent::truncated_diff(const std::string_view diff) -> std::string {
	std::size_t kept = 0;
	std::size_t offset = 0;
	while (kept < draft_diff_lines) {
		const std::size_t newline = diff.find('\n', offset);
		if (newline == std::string_view::npos) {
			return std::string(diff);
		}
		offset = newline + 1;
		++kept;
	}
	if (offset >= diff.size()) {
		return std::string(diff);
	}
	return std::format("{}[diff truncated after {} lines]\n", diff.substr(0, offset), draft_diff_lines);
}

auto gse::ide::agent::draft_context(const draft_request& request, const std::string_view subjects, const std::string_view diff) -> std::string {
	std::string context = std::format("repository {}\nbranch {}\n\nselected files\n", request.root.filename().generic_display_string(), request.branch);
	for (const draft_file& file : request.files) {
		context += std::format("  {} {} +{} -{}\n", file.code, file.relative.generic_display_string(), file.added, file.deleted);
	}

	if (!subjects.empty()) {
		context += "\nrecent commit subjects\n";
		for (const auto subject : std::views::split(subjects, '\n')) {
			const std::string_view line(subject.begin(), subject.end());
			if (!line.empty()) {
				context += std::format("  {}\n", line);
			}
		}
	}

	context += "\ndiff\n";
	if (diff.empty()) {
		context += "[no tracked changes in the selection - the files above are new, so judge them by their paths and line counts]\n";
		return context;
	}
	context += truncated_diff(diff);
	return context;
}

auto gse::ide::agent::attribution_line(const std::string_view line) -> bool {
	std::string lowered;
	lowered.reserve(line.size());
	for (const char letter : line) {
		lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
	}
	return lowered.starts_with("co-authored-by:")
		|| lowered.contains("generated with")
		|| lowered.contains("claude code")
		|| lowered.contains("\xf0\x9f\xa4\x96");
}

auto gse::ide::agent::cleaned_message(const std::span<const std::string> lines) -> std::string {
	std::vector<std::string_view> kept;
	for (const std::string& line : lines) {
		if (line.starts_with("```") || attribution_line(line)) {
			continue;
		}
		kept.push_back(line);
	}
	while (!kept.empty() && kept.front().find_first_not_of(" \t") == std::string_view::npos) {
		kept.erase(kept.begin());
	}
	while (!kept.empty() && kept.back().find_first_not_of(" \t") == std::string_view::npos) {
		kept.pop_back();
	}

	std::string message;
	for (const std::string_view line : kept) {
		if (!message.empty()) {
			message += '\n';
		}
		message += line;
	}
	return message;
}

auto gse::ide::agent::run_claude(const std::filesystem::path& root, const std::string_view model, const std::string_view prompt, const std::stop_token& cancel) -> std::expected<std::vector<std::string>, std::string> {
	const credentials creds = agent_credentials();
	if (!creds.token) {
		return std::unexpected("no CLAUDE_CODE_OAUTH_TOKEN in this process or in the user environment - run `claude setup-token`");
	}

	std::string command(draft_command);
	if (!model.empty()) {
		command += " --model ";
		command += model;
	}
	command.append(draft_denied_tools);

	const spawn::launched child = spawn::launch_streamed(std::wstring(command.begin(), command.end()), root.wstring(), creds.environment);
	if (!win32::valid_handle(child.process)) {
		return std::unexpected("failed to launch 'claude' - is it on PATH?");
	}
	const auto _ = make_scope_exit([&child] {
		spawn::terminate(child.process, child.job);
		for (void* handle : { child.process, child.job, child.output }) {
			if (win32::valid_handle(handle)) {
				win32::CloseHandle(handle);
			}
		}
	});

	win32::DWORD written = 0;
	const bool sent = win32::WriteFile(child.input, prompt.data(), static_cast<win32::DWORD>(prompt.size()), &written, nullptr);
	win32::CloseHandle(child.input);
	if (!sent || written != prompt.size()) {
		return std::unexpected("could not send the change to claude");
	}

	const time draft_limit = seconds(180.f);
	const clock waited;
	std::string trailing;
	std::vector<std::string> lines;
	while (spawn::read_lines(child.output, trailing, lines)) {
		if (cancel.stop_requested()) {
			return std::unexpected("the draft was cancelled");
		}
		if (waited.elapsed() >= draft_limit) {
			return std::unexpected(std::format("claude did not answer within {:.0f} seconds", draft_limit.as<seconds>()));
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	win32::WaitForSingleObject(child.process, win32::infinite);
	win32::DWORD code = 0;
	win32::GetExitCodeProcess(child.process, &code);
	if (code != 0) {
		return std::unexpected(lines.empty()
			? std::format("claude exited with code {}", code)
			: std::format("claude exited with code {}: {}", code, lines.back()));
	}
	return lines;
}

auto gse::ide::agent::draft_commit_message(const draft_request& request, const std::string_view model, const std::stop_token& cancel) -> draft_ready {
	if (request.files.empty()) {
		return {
			.root = request.root,
			.failure = "select the files to describe first",
		};
	}

	const std::expected<std::string, std::string> diff = selected_diff(request);
	if (!diff) {
		return {
			.root = request.root,
			.failure = diff.error(),
		};
	}

	const std::expected<std::string, std::string> subjects = git::capture(
		std::format("git --no-optional-locks log --format=%s -n {}", draft_recent_subjects),
		request.root
	);

	const std::string recent = subjects ? *subjects : std::string{};
	const std::string prompt = std::string(draft_instructions()) + draft_context(request, recent, *diff);
	const std::expected<std::vector<std::string>, std::string> answer = run_claude(request.root, model, prompt, cancel);
	if (!answer) {
		return {
			.root = request.root,
			.failure = answer.error(),
		};
	}

	std::string message = cleaned_message(*answer);
	if (message.empty()) {
		return {
			.root = request.root,
			.failure = "claude returned no commit message",
		};
	}
	return {
		.root = request.root,
		.message = std::move(message),
	};
}
