#include "mp3.h"
#include "setup.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3/minimp3.h"

#define CASE_BIT_RATE(x, z)\
case x:\
	bit_rate = z;\
	break

#define CASE_SAMPLE_RATE(x, z)\
case x:\
	sample_rate = z;\
	break

namespace Audio
{
	namespace MP3
	{
		Header ReadHeader(FILE* fp)
		{
			Header header;
			//Skipping tags v1 and v2 atm
			fread(&header, sizeof(Header), 1, fp);
			SwapBytes(header.hex);
			return header;
		}

		void ExtractDirectsoundData(Header header, uint32_t& sample_rate, uint32_t& channels, uint32_t& bits_per_sample)
		{
			sample_rate = header.freq_index ? (header.freq_index == 1 ? 48000 : 32000) : 44100;
			channels = header.channel_type != 3 ? 2 : 1;
			bits_per_sample = 8 * sizeof(int16_t);
		}

		uint32_t ComputeFrameLength(Header header)
		{
			uint32_t bit_rate = 0, sample_rate = 0;
			switch (header.bitrate_index) {
				CASE_BIT_RATE(0b0000, 0);
				CASE_BIT_RATE(0b0001, 32000);
				CASE_BIT_RATE(0b0010, 40000);
				CASE_BIT_RATE(0b0011, 48000);
				CASE_BIT_RATE(0b0100, 56000);
				CASE_BIT_RATE(0b0101, 64000);
				CASE_BIT_RATE(0b0110, 80000);
				CASE_BIT_RATE(0b0111, 96000);
				CASE_BIT_RATE(0b1000, 112000);
				CASE_BIT_RATE(0b1001, 128000);
				CASE_BIT_RATE(0b1010, 160000);
				CASE_BIT_RATE(0b1011, 192000);
				CASE_BIT_RATE(0b1100, 224000);
				CASE_BIT_RATE(0b1101, 256000);
				CASE_BIT_RATE(0b1110, 320000);
				CASE_BIT_RATE(0b1111, 0);
			}

			switch (header.freq_index) {
				CASE_SAMPLE_RATE(0b00, 44100);
				CASE_SAMPLE_RATE(0b01, 48000);
				CASE_SAMPLE_RATE(0b10, 32000);
				CASE_SAMPLE_RATE(0b11, 0);
			}

			return static_cast<uint32_t>(144 * ((float)bit_rate / (float)sample_rate) + header.padding);
		}

		Metadata Load(const std::string& str)
		{
			FILE* fp;
			if (fopen_s(&fp, str.c_str(), "rb") != 0) {
				return {};
			}

			//Copy the data into the buffer
			Metadata data;
			fseek(fp, 0, SEEK_END);
			uint32_t file_size = ftell(fp);
			data.filebuf = std::make_unique<uint8_t[]>(file_size);
			fseek(fp, 0, SEEK_SET);
			fread(data.filebuf.get(), 1, file_size, fp);
			fseek(fp, 0, SEEK_SET);

			uint32_t file_cursor = 0;
			char tag_mark[4] = {};
			uint8_t* filebuf = data.filebuf.get();
			while (!feof(fp)) {
				fread(tag_mark, 1, 3, fp);
				if (std::strcmp(tag_mark, "TAG") == 0 || std::strcmp(tag_mark, "ID3") == 0) {
					if (std::strcmp(tag_mark, "TAG") == 0) {
						file_cursor += 128;
					}
					else {
						fseek(fp, file_cursor + 0x6, SEEK_SET);
						uint32_t to_jump;
						fread(&to_jump, 4, 1, fp);
						SwapBytes(to_jump);
						file_cursor += 0xA + to_jump;
					}
				}
				fseek(fp, file_cursor, SEEK_SET);
				data.frames.push_back({});
				auto& frame = data.frames.back();
				Header header = ReadHeader(fp);

				if (header.frame_synchronizer != 0b11111111111 || header.mpeg_ver_id != 3 || header.layer != 1) {
					//Unusual not supported mp3 format, at the moment at least
					data.frames.pop_back();
					data.max_pcm_samples = data.frames.size() * MINIMP3_MAX_SAMPLES_PER_FRAME;
					return data;
				}

				if (data.first_header.hex == 0)
					data.first_header = header;

				uint32_t header_position = file_cursor;
				uint32_t frame_length = ComputeFrameLength(header);
				frame.start.ptr = filebuf + header_position;
				frame.start.size = frame_length;
				file_cursor = header_position + frame_length;
			}

			fclose(fp);
			data.max_pcm_samples = data.frames.size() * MINIMP3_MAX_SAMPLES_PER_FRAME;
			return data;
		}

		void Decode(const Metadata& data, int16_t* buf, uint32_t size)
		{
			//Using the minimp3 lib for now, eventually i will try and write manually
			//an mp3 decoder myself as an exercise
			mp3dec_t decoder;
			mp3dec_init(&decoder);

			uint32_t cursor = 0;
			for (auto& frame : data.frames) {

				if (cursor >= size) {
					//LOG_ERROR("Going over the buf boundaries");
					return;
				}

				mp3dec_frame_info_t info;
				mp3dec_decode_frame(&decoder, frame.start.ptr, frame.start.size, buf + cursor, &info);
				cursor += MINIMP3_MAX_SAMPLES_PER_FRAME;
			}
		}
	}
}

#undef CASE_BIT_RATE
#undef CASE_SAMPLE_RATE