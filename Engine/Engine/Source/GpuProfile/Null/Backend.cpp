module gse.gpu_profile;

import std;

import gse.math;

import :backend;
import :session;

auto gse::gpu_profile::backend::start_session(session_state&, const session_settings&) -> session_status {
	return session_status::not_compiled_in;
}

auto gse::gpu_profile::backend::stop_session() -> void {}

auto gse::gpu_profile::backend::raw_gpu_timestamp() -> std::optional<time_t<std::uint64_t>> {
	return std::nullopt;
}

auto gse::gpu_profile::backend::decode_samples(session_state&) -> void {}
