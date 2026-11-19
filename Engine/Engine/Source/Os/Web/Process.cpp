module gse.os:process_impl;

import std;

import gse.math;

import :process;

gse::os::unique_handle::unique_handle(void* const handle) : m_handle(handle) {}

gse::os::unique_handle::unique_handle(unique_handle&& other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}

auto gse::os::unique_handle::operator=(unique_handle&& other) noexcept -> unique_handle& {
	m_handle = std::exchange(other.m_handle, nullptr);
	return *this;
}

gse::os::unique_handle::~unique_handle() = default;

auto gse::os::unique_handle::valid() const -> bool {
	return m_handle != nullptr;
}

auto gse::os::unique_handle::handle() const -> void* {
	return m_handle;
}

auto gse::os::unique_handle::release() -> void* {
	return std::exchange(m_handle, nullptr);
}

auto gse::os::spawn_in_job(const spawn_request&) -> std::optional<spawned_process> {
	return std::nullopt;
}

auto gse::os::watch_process(std::uint32_t) -> std::optional<watched_process> {
	return std::nullopt;
}

auto gse::os::exit_code_of(const unique_handle&) -> std::optional<std::uint32_t> {
	return std::nullopt;
}

auto gse::os::exit_code_of(const spawned_process& spawned) -> std::optional<std::uint32_t> {
	return exit_code_of(spawned.process);
}

auto gse::os::terminate(const spawned_process&) -> void {}

auto gse::os::open_inheritable(const std::filesystem::path&, std::uint32_t, std::uint32_t) -> unique_handle {
	return {};
}

auto gse::os::create_output(const std::filesystem::path&) -> unique_handle {
	return {};
}

auto gse::os::try_lock_file(const std::filesystem::path&) -> std::optional<unique_handle> {
	return std::nullopt;
}

auto gse::os::available_commit() -> byte_count {
	return bytes(0.);
}

auto gse::os::current_executable() -> std::filesystem::path {
	return {};
}
