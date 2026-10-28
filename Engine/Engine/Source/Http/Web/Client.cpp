module gse.http:client_platform;

import gse.log;
import std;

import :client;
import :request;

auto gse::http::open_session(std::string_view) -> native_session {
	log::println(log::level::warning, log::category::http, "no http backend on this platform; every request will fail");
	return nullptr;
}

auto gse::http::close_session(native_session) -> void {}

auto gse::http::perform(native_session, const request&, const std::atomic<bool>&) -> result {
	return std::unexpected(error::unsupported_platform);
}
