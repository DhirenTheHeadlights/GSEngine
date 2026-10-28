module;

#include <miniaudio.h>

export module gse.miniaudio;

export {
	using ::ma_decoder;
	using ::ma_decoder_config;
	using ::ma_sound;
	using ::ma_sound_config;
	using ::ma_engine;
	using ::ma_engine_config;
	using ::ma_data_source;
	using ::ma_format;
	using ::ma_result;
	using ::ma_uint8;
	using ::ma_uint16;
	using ::ma_uint32;
	using ::ma_uint64;
	using ::ma_int32;
	using ::ma_bool32;

	using ::ma_decoder_config_init;
	using ::ma_decoder_init_memory;
	using ::ma_decoder_uninit;
	using ::ma_decoder_get_length_in_pcm_frames;
	using ::ma_sound_init_from_data_source;
	using ::ma_sound_uninit;
	using ::ma_sound_start;
	using ::ma_sound_stop;
	using ::ma_sound_set_looping;
	using ::ma_sound_set_volume;
	using ::ma_sound_at_end;
	using ::ma_engine_init;
	using ::ma_engine_uninit;
	using ::ma_engine_set_volume;
	using ::ma_engine_config_init;

	inline constexpr ::ma_result ma_result_success = MA_SUCCESS;
	inline constexpr ::ma_format ma_format_value_unknown = ma_format_unknown;
	inline constexpr ::ma_format ma_format_value_f32 = ma_format_f32;
	inline constexpr ::ma_bool32 ma_bool_true = MA_TRUE;
	inline constexpr ::ma_bool32 ma_bool_false = MA_FALSE;
}
