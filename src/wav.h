#pragma once
#include <stdint.h>
#include <iostream>

namespace Audio
{
	namespace WAV 
	{
		//Header of supported formats
		struct Header
		{
			uint8_t riff_str[4];
			uint32_t file_size;
			uint8_t wave_str[4];
			uint8_t fmt_str[4];
			uint32_t format_data;
			uint16_t format_type;
			uint16_t channels;
			uint32_t sample_rate;
			uint32_t byte_rate;
			uint16_t unused;
			uint16_t bits_per_sample;
			uint8_t data_str[4];
			uint32_t data_size;
		};

		//WAV functions
		Header ReadHeader(FILE* fp);
	}
}