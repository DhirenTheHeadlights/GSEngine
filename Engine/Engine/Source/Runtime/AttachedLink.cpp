module gse.runtime:attached_link_impl;

import std;

import :attached_link;
import :engine;
import :frame_pacing;

import gse.core;
import gse.log;
import gse.win32;

auto gse::drain_editor_pipe(win32::HANDLE& editor_pipe, attached_pipe_reader& reader, engine& e, frame_pacing& pacing) -> void {
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

		if (!reader.have_magic) {
			std::uint32_t magic = 0;
			std::memcpy(&magic, reader.bytes.data(), sizeof(magic));
			std::size_t total = 0;
			if (magic == attached_input_magic) {
				total = sizeof(attached_input_message);
			}
			else if (magic == attached_pacing_magic) {
				total = sizeof(attached_pacing_message);
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

		std::uint32_t magic = 0;
		std::memcpy(&magic, reader.bytes.data(), sizeof(magic));
		if (magic == attached_input_magic) {
			attached_input_message message{};
			std::memcpy(&message, reader.bytes.data(), sizeof(message));
			e.push_attached_input(message.event);
		}
		else {
			attached_pacing_message message{};
			std::memcpy(&message, reader.bytes.data(), sizeof(message));
			pacing.refresh = message.refresh;
		}
		reader.received = 0;
		reader.expected = sizeof(std::uint32_t);
		reader.have_magic = false;
	}
}
