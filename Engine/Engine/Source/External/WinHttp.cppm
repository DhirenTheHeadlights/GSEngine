module;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#endif

#undef assert

export module gse.winhttp;

import std;

#ifdef _WIN32
export {
	using ::HINTERNET;

	using ::WinHttpOpen;
	using ::WinHttpCloseHandle;
	using ::WinHttpConnect;
	using ::WinHttpOpenRequest;
	using ::WinHttpAddRequestHeaders;
	using ::WinHttpSendRequest;
	using ::WinHttpReceiveResponse;
	using ::WinHttpQueryHeaders;
	using ::WinHttpQueryDataAvailable;
	using ::WinHttpReadData;
	using ::WinHttpSetOption;
	using ::WinHttpSetTimeouts;
	using ::GetLastError;
}
#endif

export namespace gse::winhttp {
#ifdef _WIN32
	using handle = ::HINTERNET;

	constexpr bool supported = true;

	constexpr unsigned long access_type_automatic_proxy = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
	constexpr unsigned long access_type_no_proxy = WINHTTP_ACCESS_TYPE_NO_PROXY;
	constexpr unsigned long flag_secure = WINHTTP_FLAG_SECURE;
	constexpr unsigned long addreq_flag_add = WINHTTP_ADDREQ_FLAG_ADD;
	constexpr unsigned long addreq_flag_replace = WINHTTP_ADDREQ_FLAG_REPLACE;
	constexpr unsigned long option_decompression = WINHTTP_OPTION_DECOMPRESSION;
	constexpr unsigned long decompression_flag_all = WINHTTP_DECOMPRESSION_FLAG_ALL;
	constexpr unsigned long option_enable_feature = WINHTTP_OPTION_ENABLE_FEATURE;
	constexpr unsigned long enable_ssl_revocation = WINHTTP_ENABLE_SSL_REVOCATION;
	constexpr unsigned long option_secure_protocols = WINHTTP_OPTION_SECURE_PROTOCOLS;
	constexpr unsigned long secure_protocol_tls1_2 = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
	constexpr unsigned long secure_protocol_tls1_3 = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
	constexpr unsigned long query_status_code = WINHTTP_QUERY_STATUS_CODE;
	constexpr unsigned long query_raw_headers_crlf = WINHTTP_QUERY_RAW_HEADERS_CRLF;
	constexpr unsigned long query_flag_number = WINHTTP_QUERY_FLAG_NUMBER;
	constexpr unsigned long error_insufficient_buffer = ERROR_INSUFFICIENT_BUFFER;
	constexpr unsigned long error_timeout = ERROR_WINHTTP_TIMEOUT;
	constexpr unsigned long error_operation_cancelled = ERROR_WINHTTP_OPERATION_CANCELLED;
	constexpr unsigned long error_cannot_connect = ERROR_WINHTTP_CANNOT_CONNECT;
	constexpr unsigned long error_name_not_resolved = ERROR_WINHTTP_NAME_NOT_RESOLVED;
#else
	constexpr bool supported = false;

	using HINTERNET = void*;

	constexpr unsigned long access_type_automatic_proxy = 0;
	constexpr unsigned long access_type_no_proxy = 0;
	constexpr unsigned long flag_secure = 0;
	constexpr unsigned long addreq_flag_add = 0;
	constexpr unsigned long addreq_flag_replace = 0;
	constexpr unsigned long option_decompression = 0;
	constexpr unsigned long decompression_flag_all = 0;
	constexpr unsigned long option_enable_feature = 0;
	constexpr unsigned long enable_ssl_revocation = 0;
	constexpr unsigned long option_secure_protocols = 0;
	constexpr unsigned long secure_protocol_tls1_2 = 0;
	constexpr unsigned long secure_protocol_tls1_3 = 0;
	constexpr unsigned long query_status_code = 0;
	constexpr unsigned long query_raw_headers_crlf = 0;
	constexpr unsigned long query_flag_number = 0;
	constexpr unsigned long error_insufficient_buffer = 0;
	constexpr unsigned long error_timeout = 0;
	constexpr unsigned long error_operation_cancelled = 0;
	constexpr unsigned long error_cannot_connect = 0;
	constexpr unsigned long error_name_not_resolved = 0;
#endif
}
