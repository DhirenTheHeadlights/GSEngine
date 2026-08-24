export module gse.http:client;

import std;

import gse.core;

import :request;

export namespace gse::http {
	struct completion {
		gse::id ticket;
		http::result value;
	};

	class client : public non_copyable {
	public:
		static constexpr std::size_t default_worker_count = 2;

		client();

		explicit client(
			std::string_view user_agent
		);

		client(
			std::string_view user_agent,
			std::size_t worker_count
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
