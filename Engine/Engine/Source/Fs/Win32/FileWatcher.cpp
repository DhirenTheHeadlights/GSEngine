module gse.fs:file_watcher_platform;

import gse.win32;
import std;

import :file_watcher;

namespace gse {
	constexpr std::size_t notify_buffer_bytes = 64 * 1024;

	constexpr win32::DWORD notify_filter =
		win32::file_notify_change_file_name |
		win32::file_notify_change_dir_name |
		win32::file_notify_change_last_write |
		win32::file_notify_change_size;

	struct directory_stream {
		win32::HANDLE handle = nullptr;
		win32::OVERLAPPED overlapped{};
		std::vector<std::byte> buffer = std::vector<std::byte>(notify_buffer_bytes);
		std::filesystem::path directory;
	};
}

auto gse::file_watcher::open_stream(watch_entry& entry) -> void {
	auto stream = std::make_unique<directory_stream>();
	stream->directory = entry.path;
	stream->handle = win32::CreateFileW(
		entry.path.native().c_str(),
		win32::file_list_directory,
		win32::file_share_read | win32::file_share_write | win32::file_share_delete,
		nullptr,
		win32::open_existing,
		win32::file_flag_backup_semantics | win32::file_flag_overlapped,
		nullptr
	);

	if (!win32::valid_handle(stream->handle)) {
		return;
	}

	const win32::HANDLE port = win32::CreateIoCompletionPort(
		stream->handle,
		m_completion_port,
		reinterpret_cast<win32::ULONG_PTR>(stream.get()),
		0
	);
	if (!port) {
		win32::CloseHandle(stream->handle);
		return;
	}
	m_completion_port = port;

	entry.stream = stream.release();
	if (!arm_stream(entry)) {
		close_stream(entry);
	}
}

auto gse::file_watcher::close_stream(watch_entry& entry) -> void {
	auto* stream = static_cast<directory_stream*>(entry.stream);
	if (!stream) {
		return;
	}
	if (win32::valid_handle(stream->handle)) {
		win32::CancelIoEx(stream->handle, &stream->overlapped);
		win32::CloseHandle(stream->handle);
	}
	delete stream;
	entry.stream = nullptr;
}

auto gse::file_watcher::arm_stream(watch_entry& entry) -> bool {
	auto* stream = static_cast<directory_stream*>(entry.stream);
	if (!stream || !win32::valid_handle(stream->handle)) {
		return false;
	}
	stream->overlapped = {};
	return win32::ReadDirectoryChangesW(
		stream->handle,
		stream->buffer.data(),
		static_cast<win32::DWORD>(stream->buffer.size()),
		entry.recursive,
		notify_filter,
		nullptr,
		&stream->overlapped,
		nullptr
	) != 0;
}

auto gse::file_watcher::drain_completions() -> std::size_t {
	if (!m_completion_port) {
		return 0;
	}

	std::size_t changes = 0;

	for (;;) {
		win32::DWORD bytes = 0;
		win32::ULONG_PTR key = 0;
		win32::OVERLAPPED* overlapped = nullptr;
		if (!win32::GetQueuedCompletionStatus(m_completion_port, &bytes, &key, &overlapped, 0)) {
			break;
		}

		watch_entry* entry = entry_for_stream(reinterpret_cast<const void*>(key));
		if (!entry) {
			continue;
		}
		auto* stream = static_cast<directory_stream*>(entry->stream);

		if (bytes == 0) {
			changes += rescan_entry(*entry);
			arm_stream(*entry);
			continue;
		}

		std::size_t offset = 0;
		for (;;) {
			const auto* record = reinterpret_cast<const win32::FILE_NOTIFY_INFORMATION*>(stream->buffer.data() + offset);
			const std::wstring name(record->FileName, record->FileNameLength / sizeof(wchar_t));
			changes += emit_change(*entry, entry->path / name);

			if (record->NextEntryOffset == 0) {
				break;
			}
			offset += record->NextEntryOffset;
			if (offset >= stream->buffer.size()) {
				break;
			}
		}

		arm_stream(*entry);
	}

	return changes;
}

auto gse::file_watcher::destroy_completion_port() -> void {
	if (m_completion_port) {
		win32::CloseHandle(m_completion_port);
		m_completion_port = nullptr;
	}
}
