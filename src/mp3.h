#pragma once
#include <stdint.h>
#include <vector>
#include <memory>
#include <string>

namespace Audio
{
	namespace MP3
	{
		union Header
		{
			struct {
				uint32_t emphasis : 2;
				uint32_t original : 1;
				uint32_t copyright : 1;
				uint32_t mode_extension : 2;
				uint32_t channel_type : 2;
				uint32_t private_bit : 1;
				uint32_t padding : 1;
				uint32_t freq_index : 2;
				uint32_t bitrate_index : 4;
				uint32_t crc_protection : 1;
				uint32_t layer : 2;
				uint32_t mpeg_ver_id : 2;
				uint32_t frame_synchronizer : 11;
			};
			uint32_t hex;
		};

		struct Section
		{
			uint8_t* ptr;
			uint32_t size;
		};

		struct Frame
		{
			Section start;
		};

		//no v1/v2 tags stored at the moment
		struct Metadata
		{
			std::unique_ptr<uint8_t[]> filebuf;
			std::vector<Frame> frames;
			Header first_header{};
			uint32_t max_pcm_samples = 0;
		};

		//MP3 functions
		Header ReadHeader(FILE* fp);
		void ExtractDirectsoundData(Header header, uint32_t& sample_rate, uint32_t& channels, uint32_t& bits_per_sample);
		uint32_t ComputeFrameLength(Header header);
		Metadata Load(const std::string& str);
		void Decode(const Metadata& data, int16_t* buf, uint32_t size);
	}
}