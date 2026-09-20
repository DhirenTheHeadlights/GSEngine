export module gse.fs:file_watcher;

import std;

import gse.math;
import gse.time;

export namespace gse {
	class file_watcher {
	public:
		using callback = std::function<void(const std::filesystem::path&)>;
		using directory_filter = std::function<bool(const std::filesystem::path&)>;

		file_watcher() = default;

		~file_watcher();

		file_watcher(const file_watcher&) = delete;

		auto operator=(const file_watcher&) -> file_watcher& = delete;

		auto watch(
			const std::filesystem::path& path,
			callback on_change
		) -> void;

		auto watch_directory(
			const std::filesystem::path& directory,
			callback on_change,
			std::span<const std::string> extensions = {},
			bool recursive = true,
			directory_filter recurse_into = {}
		) -> void;

		auto unwatch(
			const std::filesystem::path& path
		) -> void;

		auto poll() -> std::size_t;
		auto poll_now() -> std::size_t;

		auto clear() -> void;

	private:
		struct watch_entry {
			std::filesystem::path path;
			std::filesystem::file_time_type last_modified;
			callback on_change;
			bool is_directory;
			bool recursive;
			std::vector<std::string> extensions;
			directory_filter recurse_into;
			void* stream = nullptr;
		};

		std::vector<watch_entry> m_watches;
		std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> m_directory_files;
		mutable std::mutex m_mutex;
		interval_timer<> m_poll_timer{ milliseconds(500.f) };
		void* m_completion_port = nullptr;

		static auto matches_extensions(
			const std::filesystem::path& path,
			std::span<const std::string> extensions
		) -> bool;

		static auto scan_directory(
			const std::filesystem::path& directory,
			std::span<const std::string> extensions,
			bool recursive,
			const directory_filter& recurse_into
		) -> std::unordered_map<std::filesystem::path, std::filesystem::file_time_type>;

		auto open_stream(
			watch_entry& entry
		) -> void;

		auto close_stream(
			watch_entry& entry
		) -> void;

		auto arm_stream(
			watch_entry& entry
		) -> bool;

		auto entry_for_stream(
			const void* stream
		) -> watch_entry*;

		auto emit_change(
			watch_entry& entry,
			const std::filesystem::path& path
		) -> std::size_t;

		auto rescan_entry(
			watch_entry& entry
		) -> std::size_t;

		auto drain_completions() -> std::size_t;

		auto poll_locked() -> std::size_t;
	};
}
