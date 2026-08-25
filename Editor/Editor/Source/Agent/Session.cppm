export module gse.ide.agent:session;

import std;
import gse;

import :model;

namespace gse::ide::agent {
	constexpr std::string_view agent_command = "claude -p --output-format stream-json --input-format stream-json --verbose --permission-mode auto";
	constexpr std::wstring_view resume_option = L" --resume ";
	constexpr std::wstring_view oauth_token_name = L"CLAUDE_CODE_OAUTH_TOKEN";
	constexpr std::wstring_view config_dir_name = L"CLAUDE_CONFIG_DIR";
	constexpr std::wstring_view user_profile_name = L"USERPROFILE";
	constexpr std::wstring_view handoff_option = L"--agent-handoff=";
	constexpr std::uint32_t sessions_magic = 0x47534147;
	constexpr std::uint32_t sessions_version = 5;
	constexpr std::uint32_t first_schema_sessions_version = 3;
	constexpr std::uint32_t chat_names_sessions_version = 5;

	struct handoff {
		std::uint32_t session = 0;
		void* process = nullptr;
		void* job = nullptr;
		void* output = nullptr;
		void* input = nullptr;
	};

	struct credentials {
		std::vector<wchar_t> environment;
		bool token = false;
	};

	auto agent_credentials() -> credentials;

	auto sessions_path() -> std::filesystem::path;

	auto save_sessions(
		const data& d
	) -> void;

	auto load_sessions(
		data& d
	) -> void;

	auto attach_runtime(
		session& s
	) -> void;

	auto inherited_handoffs() -> std::vector<handoff>;

	auto adopt_inherited(
		data& d
	) -> void;

	auto inherited_handle(
		void* handle
	) -> bool;

	auto discard_handoff(
		const handoff& adopted
	) -> void;

	auto adopt_session(
		session& s,
		const handoff& adopted
	) -> bool;

	auto hand_off_session(
		session& s
	) -> bool;

	auto create_session(
		data& d,
		const std::filesystem::path& cwd
	) -> session&;

	auto session_command(
		const session& s
	) -> std::wstring;

	auto launch_session(
		session& s
	) -> bool;

	auto append_row(
		session& s,
		transcript_row row
	) -> void;

	auto pump_session(
		session& s
	) -> void;

	auto close_session(
		session& s
	) -> void;

	auto erase_session(
		data& d,
		std::uint32_t session_id
	) -> void;

	auto request_close(
		data& d,
		std::uint32_t session_id
	) -> void;

	auto send_to_session(
		session& s,
		std::string_view prompt,
		std::span<const attachment> attachments
	) -> void;

	auto interrupt_session(
		session& s
	) -> void;

	auto active_session(
		data& d
	) -> session*;

	auto environment_path(
		std::wstring_view name
	) -> std::filesystem::path;

	auto claude_home() -> std::filesystem::path;
}
