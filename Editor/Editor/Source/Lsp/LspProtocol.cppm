export module ide:lsp_protocol;

import std;
import gse;

export namespace ide::lsp {
	template <gse::fixed_string Method>
	struct message {
		static constexpr std::string_view method = Method;
		static constexpr bool is_notification = false;
	};

	template <gse::fixed_string Method>
	struct notification {
		static constexpr std::string_view method = Method;
		static constexpr bool is_notification = true;
	};

	struct optional_field {};

	struct position {
		std::uint32_t line = 0;
		std::uint32_t character = 0;
	};

	struct range {
		position start;
		position end;
	};

	struct text_document_identifier {
		std::string uri;
	};

	struct versioned_text_document_identifier {
		std::string uri;
		std::int32_t version = 0;
	};

	struct text_document_item {
		std::string uri;
		std::string language_id;
		std::int32_t version = 0;
		std::string text;
	};

	struct text_document_position_params {
		text_document_identifier text_document;
		position position;
	};

	struct [[= message<"initialize">{}]] initialize_request {
		std::int32_t process_id = 0;
		std::string root_uri;
		std::string client_name = "GSEditor";
	};

	struct [[= notification<"textDocument/didOpen">{}]] did_open_notification {
		text_document_item text_document;
	};

	struct [[= notification<"textDocument/didChange">{}]] did_change_notification {
		versioned_text_document_identifier text_document;
		std::string full_text;
	};

	struct [[= message<"textDocument/completion">{}]] completion_request {
		text_document_position_params params;
	};

	struct diagnostic {
		range range;
		std::int32_t severity = 0;
		std::string message;
		std::string source;
	};

	struct [[= notification<"textDocument/publishDiagnostics">{}]] publish_diagnostics_notification {
		std::string uri;
		std::vector<diagnostic> diagnostics;
	};
}
