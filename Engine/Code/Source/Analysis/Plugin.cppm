export module gse.analysis:plugin;

import std;

export namespace gse::analysis {
	struct plugin_request {
		std::filesystem::path plugin;
		std::filesystem::path out;
		std::span<const std::filesystem::path> roots;
		bool complete = false;
		bool audit = false;
	};

	auto plugin_arguments(
		const plugin_request& request
	) -> std::string;
}

namespace gse::analysis {
	auto append_path_argument(
		std::string& into,
		std::string_view flag,
		const std::filesystem::path& path
	) -> void;
}

auto gse::analysis::append_path_argument(std::string& into, const std::string_view flag, const std::filesystem::path& path) -> void {
	into += ' ';
	into += flag;
	into += "=\"";
	into += path.generic_native_encoded_string();
	into += '"';
}

auto gse::analysis::plugin_arguments(const plugin_request& request) -> std::string {
	std::string out;
	append_path_argument(out, "-fplugin", request.plugin);
	append_path_argument(out, "-fplugin-arg-gse_tokens-out", request.out);
	for (const std::filesystem::path& root : request.roots) {
		append_path_argument(out, "-fplugin-arg-gse_tokens-root", root);
	}
	if (request.complete) {
		out += " -fplugin-arg-gse_tokens-complete";
	}
	if (request.audit) {
		out += " -fplugin-arg-gse_tokens-audit";
	}
	return out;
}
