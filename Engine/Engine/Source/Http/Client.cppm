export module gse.http:client;

import std;

import gse.core;

import :request;

export namespace gse::http {
	struct completion {
		id ticket;
		result value;
	};

	class client : public non_copyable {
	public:
		static constexpr std::size_t max_concurrent_requests = 2;

		client();

		explicit client(
			std::string_view user_agent
		);

		~client();

		client(
			client&& other
		) noexcept;

		auto operator=(
			client&& other
		) noexcept -> client&;

		auto send(
			request req
		) -> id;

		auto poll() -> std::vector<completion>;

		[[nodiscard]] auto in_flight() const -> std::size_t;

		[[nodiscard]] auto capacity() const -> std::size_t;

		[[nodiscard]] auto valid() const -> bool;

	private:
		struct session;

		std::shared_ptr<session> m_session;
	};
}

namespace gse::http {
	using native_session = void*;

	auto open_session(
		std::string_view user_agent
	) -> native_session;

	auto close_session(
		native_session handle
	) -> void;

	auto perform(
		native_session session_handle,
		const request& req,
		const std::atomic<bool>& cancelled
	) -> result;
}