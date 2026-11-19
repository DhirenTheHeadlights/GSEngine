export module gse.sdk:feed;

import std;

import gse.json;

export namespace gse::sdk {
	constexpr std::string_view feed_name = "feed.json";

	struct feed {
		std::string product;
		std::string version;
		std::string preset;
		std::string asset;
		std::string url;
		std::string sha256;
		std::uint64_t size = 0;
		std::string server;
	};

	struct release_version {
		std::uint32_t year = 0;
		std::uint32_t day = 0;
		std::uint32_t ordinal = 0;

		auto operator<=>(
			const release_version&
		) const = default;
	};

	auto parse_version(
		std::string_view text
	) -> std::optional<release_version>;

	auto is_newer(
		std::string_view candidate,
		std::string_view installed
	) -> bool;

	auto parse_feed(
		std::string_view text
	) -> std::expected<feed, std::string>;

	auto write_feed(
		const feed& entry
	) -> std::string;
}

auto gse::sdk::parse_version(const std::string_view text) -> std::optional<release_version> {
	if (!text.starts_with('v')) {
		return std::nullopt;
	}
	release_version parsed;
	std::uint32_t* fields[] = { &parsed.year, &parsed.day, &parsed.ordinal };
	std::size_t index = 0;
	for (const auto part : std::views::split(text.substr(1), '.')) {
		const std::string_view field(part);
		if (index == std::size(fields) || field.empty()) {
			return std::nullopt;
		}
		if (std::from_chars(field.data(), field.data() + field.size(), *fields[index]).ec != std::errc{}) {
			return std::nullopt;
		}
		++index;
	}
	return index == std::size(fields) ? std::optional(parsed) : std::nullopt;
}

auto gse::sdk::is_newer(const std::string_view candidate, const std::string_view installed) -> bool {
	const auto left = parse_version(candidate);
	const auto right = parse_version(installed);
	if (!left || !right) {
		return candidate != installed;
	}
	return *left > *right;
}

auto gse::sdk::parse_feed(const std::string_view text) -> std::expected<feed, std::string> {
	const auto parsed = json::parse_as<feed>(text);
	if (!parsed) {
		return std::unexpected(std::string(json::message(parsed.error().code)));
	}
	if (parsed->version.empty() || parsed->url.empty() || parsed->sha256.empty()) {
		return std::unexpected("the feed names no version, url and sha256");
	}
	return *parsed;
}

auto gse::sdk::write_feed(const feed& entry) -> std::string {
	return json::stringify(entry, { .indent = 2 }) + "\n";
}
