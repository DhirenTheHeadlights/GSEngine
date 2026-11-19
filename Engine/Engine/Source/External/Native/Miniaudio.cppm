module;

#include <miniaudio.h>

export module gse.miniaudio;

export {
	using ::ma_decoder_config;
	using ::ma_device;
	using ::ma_device_config;
	using ::ma_device_type;
	using ::ma_format;
	using ::ma_result;
	using ::ma_uint8;
	using ::ma_uint16;
	using ::ma_uint32;
	using ::ma_uint64;
	using ::ma_int32;
	using ::ma_bool32;

	using ::ma_decoder_config_init;
	using ::ma_decode_memory;
	using ::ma_free;
	using ::ma_device_config_init;
	using ::ma_device_init;
	using ::ma_device_uninit;
	using ::ma_device_start;
	using ::ma_device_stop;

	inline constexpr ::ma_result ma_result_success = MA_SUCCESS;
	inline constexpr ::ma_format ma_format_value_unknown = ma_format_unknown;
	inline constexpr ::ma_format ma_format_value_f32 = ma_format_f32;
	inline constexpr ::ma_device_type ma_device_type_value_playback = ma_device_type_playback;
	inline constexpr ::ma_bool32 ma_bool_true = MA_TRUE;
	inline constexpr ::ma_bool32 ma_bool_false = MA_FALSE;
}
