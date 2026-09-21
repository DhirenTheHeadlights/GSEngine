module gse.runtime:attached_link_impl;

import std;

import :attached_link;
import :engine;

import gse.core;
import gse.log;
import gse.stacktrace;
import gse.win32;

namespace gse {
	std::atomic<void*> fatal_pipe = nullptr;

	auto copy_fatal_field(
		char* field,
		std::size_t capacity,
		std::string_view text
	) -> void;
}

auto gse::copy_fatal_field(char* field, const std::size_t capacity, const std::string_view text) -> void {
	const std::size_t count = std::min(text.size(), capacity - 1);
	std::ranges::copy(text.substr(0, count), field);
	field[count] = '\0';
}

auto gse::set_fatal_pipe(const win32::HANDLE editor_pipe) -> void {
	fatal_pipe.store(editor_pipe, std::memory_order_release);
}

auto gse::report_fatal_to_editor(const std::source_location& loc, const std::string_view comment) -> void {
	auto* pipe = fatal_pipe.load(std::memory_order_acquire);
	if (!pipe) {
		return;
	}

	attached_fatal_message message{
		.magic = attached_fatal_magic,
		.line = loc.line(),
	};
	copy_fatal_field(message.file, attached_fatal_field, loc.file_name());
	copy_fatal_field(message.function, attached_fatal_field, clean_symbol(loc.function_name()));
	copy_fatal_field(message.comment, attached_fatal_field, comment);

	win32::DWORD written = 0;
	win32::WriteFile(pipe, &message, sizeof(message), &written, nullptr);
}

auto gse::drain_editor_pipe(win32::HANDLE& editor_pipe, attached_pipe_reader& reader, engine& e) -> void {
	win32::DWORD available = 0;
	while (win32::PeekNamedPipe(editor_pipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
		win32::DWORD read = 0;
		const auto remaining = static_cast<win32::DWORD>(reader.expected - reader.received);
		if (!win32::ReadFile(editor_pipe, reader.bytes.data() + reader.received, remaining, &read, nullptr) || read == 0) {
			break;
		}
		reader.received += read;
		if (reader.received < reader.expected) {
			continue;
		}

		std::uint32_t magic = 0;
		std::memcpy(&magic, reader.bytes.data(), sizeof(magic));

		if (!reader.have_magic) {
			std::size_t total = 0;
			if (magic == attached_input_magic) {
				total = sizeof(attached_input_message);
			}
			else if (magic == attached_resize_magic) {
				total = sizeof(attached_resize_message);
			}
			if (total == 0) {
				log::println(log::level::warning, log::category::general, "attached pipe: unknown message magic {:#x}; closing pipe", magic);
				win32::CloseHandle(editor_pipe);
				editor_pipe = nullptr;
				return;
			}
			reader.have_magic = true;
			reader.expected = total;
			continue;
		}

		if (magic == attached_resize_magic) {
			attached_resize_message message{};
			std::memcpy(&message, reader.bytes.data(), sizeof(message));
			e.push_attached_resize(message.extent);
		}
		else {
			attached_input_message message{};
			std::memcpy(&message, reader.bytes.data(), sizeof(message));
			e.push_attached_input(message.event);
		}

		reader.received = 0;
		reader.expected = sizeof(std::uint32_t);
		reader.have_magic = false;
	}
}
