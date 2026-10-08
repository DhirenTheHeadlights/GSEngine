export module launcher:update;

import std;

import gse;
import gse.win32;

export namespace launcher {
	struct arguments {
		bool updated = false;
		std::string connect;
	};

	struct install {
		std::filesystem::path image;
		gse::sdk::pack_stamp stamp;
		std::string feed_url;
	};

	struct progress {
		std::atomic<std::uint64_t> received{ 0 };
		std::atomic<std::uint64_t> total{ 0 };
		std::atomic<std::size_t> extracted{ 0 };
		std::atomic<std::size_t> files{ 0 };
		std::atomic<bool> extracting{ false };
	};

	auto own_executable() -> std::filesystem::path;

	auto own_install() -> std::expected<install, std::string>;

	auto game_executable(
		const install& local
	) -> std::filesystem::path;

	auto check_feed(
		const install& local
	) -> std::expected<std::optional<gse::sdk::feed>, std::string>;

	auto apply_update(
		const install& local,
		const gse::sdk::feed& entry,
		progress& reported
	) -> std::expected<std::filesystem::path, std::string>;

	auto start_game(
		const install& local,
		std::string_view connect
	) -> void;

	auto start_launcher(
		const std::filesystem::path& image,
		std::string_view connect
	) -> void;
}

namespace launcher {
	constexpr std::string_view launcher_name = "Launcher.exe";
	constexpr std::string_view game_connect_flag = "--engine-net-connect";
	constexpr std::string_view own_connect_flag = "--connect";
	constexpr std::string_view updated_flag = "--updated";
	constexpr gse::time poll_interval = gse::milliseconds(25.f);

	auto fetch(
		gse::http::request request
	) -> std::expected<gse::http::response, std::string>;
}

auto launcher::own_executable() -> std::filesystem::path {
	wchar_t buffer[gse::win32::max_path]{};
	const auto length = gse::win32::GetModuleFileNameW(nullptr, buffer, gse::win32::max_path);
	return length == 0 ? std::filesystem::path{} : std::filesystem::path(std::wstring_view(buffer, length));
}

auto launcher::own_install() -> std::expected<install, std::string> {
	const std::filesystem::path image = own_executable().parent_path().parent_path();
	const std::string manifest = gse::fs::read_text(image / "gse.manifest");
	if (manifest.empty()) {
		return std::unexpected(std::format("{} is not a packed game image (no gse.manifest)", image.generic_display_string()));
	}
	install local{
		.image = image,
		.stamp = {
			.version = gse::config::manifest_value(manifest, "version"),
			.preset = image.filename().generic_native_encoded_string(),
			.product = gse::config::manifest_value(manifest, "product"),
		},
		.feed_url = gse::config::manifest_value(manifest, "feed"),
	};
	gse::enum_from_string(gse::config::manifest_value(manifest, "kind"), local.stamp.kind);
	if (local.stamp.product.empty() || local.stamp.version.empty()) {
		return std::unexpected(std::format("{}/gse.manifest names no product and version", image.generic_display_string()));
	}
	return local;
}

auto launcher::game_executable(const install& local) -> std::filesystem::path {
	return local.image / "Bin" / (local.stamp.product + ".exe");
}

auto launcher::fetch(gse::http::request request) -> std::expected<gse::http::response, std::string> {
	gse::http::client client("GSEngine Launcher");
	if (!client.valid()) {
		return std::unexpected("no http backend is available");
	}
	const gse::id ticket = client.send(std::move(request));
	while (true) {
		for (gse::http::completion& done : client.poll()) {
			if (done.ticket != ticket) {
				continue;
			}
			if (!done.value) {
				return std::unexpected(gse::http::message(done.value.error()));
			}
			if (!done.value->ok()) {
				return std::unexpected(std::format("the server answered {}", done.value->status));
			}
			return std::move(*done.value);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(poll_interval.as<gse::milliseconds>())));
	}
}

auto launcher::check_feed(const install& local) -> std::expected<std::optional<gse::sdk::feed>, std::string> {
	if (local.feed_url.empty()) {
		return std::nullopt;
	}
	const auto answered = fetch({ .url = local.feed_url });
	if (!answered) {
		return std::unexpected(answered.error());
	}
	const auto entry = gse::sdk::parse_feed(answered->body);
	if (!entry) {
		return std::unexpected(entry.error());
	}
	if (entry->product != local.stamp.product) {
		return std::unexpected(std::format("the feed publishes {}, but this is {}", entry->product, local.stamp.product));
	}
	return *entry;
}

auto launcher::apply_update(const install& local, const gse::sdk::feed& entry, progress& reported) -> std::expected<std::filesystem::path, std::string> {
	const std::filesystem::path download = gse::config::cache_dir() / "launcher" / entry.asset;
	reported.total.store(entry.size, std::memory_order_release);
	const auto answered = fetch({
		.url = entry.url,
		.timeout = gse::seconds(60.f),
		.sink = download,
		.received = &reported.received,
	});
	if (!answered) {
		return std::unexpected(answered.error());
	}

	const auto _ = gse::make_scope_exit([&download] {
		std::error_code ignored;
		std::filesystem::remove(download, ignored);
	});

	const auto digest = gse::sdk::digest_of(download);
	if (!digest) {
		return std::unexpected(digest.error());
	}
	if (*digest != entry.sha256) {
		return std::unexpected(std::format("the download does not match the feed's hash ({} instead of {})", *digest, entry.sha256));
	}

	const auto pack = gse::sdk::read_pack(download);
	if (!pack) {
		return std::unexpected(pack.error());
	}
	if (pack->table.stamp.product != local.stamp.product || pack->table.stamp.kind != local.stamp.kind) {
		return std::unexpected(std::format("the download carries {} {}, not {} {}", gse::enum_to_string(pack->table.stamp.kind), pack->table.stamp.product, gse::enum_to_string(local.stamp.kind), local.stamp.product));
	}

	const std::filesystem::path image = gse::sdk::install_image(pack->table.stamp);
	if (image == local.image) {
		return std::unexpected(std::format("the update would overwrite the running image at {}", image.generic_display_string()));
	}
	std::error_code ec;
	std::filesystem::remove_all(image, ec);

	std::ifstream payload(pack->file, std::ios::binary);
	if (!payload) {
		return std::unexpected(std::format("could not open {}", pack->file.generic_display_string()));
	}
	reported.files.store(pack->table.entries.size(), std::memory_order_release);
	reported.extracting.store(true, std::memory_order_release);
	for (const gse::sdk::pack_entry& file : pack->table.entries) {
		if (const auto extracted = gse::sdk::extract_entry(payload, file, image); !extracted) {
			return std::unexpected(extracted.error());
		}
		reported.extracted.fetch_add(1, std::memory_order_release);
	}
	return image;
}

auto launcher::start_game(const install& local, const std::string_view connect) -> void {
	std::vector<std::filesystem::path> arguments;
	if (!connect.empty()) {
		arguments.emplace_back(game_connect_flag);
		arguments.emplace_back(connect);
	}
	gse::app::relaunch_on_exit(game_executable(local), local.image, std::move(arguments));
}

auto launcher::start_launcher(const std::filesystem::path& image, const std::string_view connect) -> void {
	std::vector<std::filesystem::path> arguments{ std::filesystem::path(updated_flag) };
	if (!connect.empty()) {
		arguments.emplace_back(own_connect_flag);
		arguments.emplace_back(connect);
	}
	gse::app::relaunch_on_exit(image / "Bin" / launcher_name, image, std::move(arguments));
}
