#include "mixer.h"
#include "win32_mixer.h"
#include "linux_mixer.h"

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
	Mixer::Mixer() :
		m_MixerMode{MixerMode::NONE}
	{
	}

	Mixer::~Mixer()
	{
	}

	std::unique_ptr<Mixer> GenerateMixer()
	{
		//TODO make this work also for other win distributions
#ifdef _MSC_VER
		return std::make_unique<Win32Mixer>();
#elif defined __linux__ || defined UNIX
		return std::make_unique<LinuxMixer>();
#else
		return std::make_unique<NullMixer>();
#endif
		//UNREACHABLE
		return nullptr;
	}



	WavHeader ReadWavHeader(FILE* fp)
	{
		WavHeader header;
		fread(&header, sizeof(WavHeader), 1, fp);

		//Ignoring non-data tags for now
		uint32_t offset = 36 + 4 + 4;
		while (std::strncmp(reinterpret_cast<const char*>(header.data_str), "data", 4) != 0) {
			offset += header.data_size;
			fseek(fp, offset, SEEK_SET);
			fread(&header.data_str, 1, 4, fp);
			fread(&header.data_size, 4, 1, fp);
		}

		return header;
	}

	Mp3Header ReadMp3Header(FILE* fp)
	{
		Mp3Header header;
		//Skipping tags v1 and v2 atm
		fread(&header, sizeof(Mp3Header), 1, fp);
		SwapBytes(header.hex);
		return header;
	}

	uint32_t ComputeMp3FrameLength(Mp3Header header)
	{
		uint32_t bit_rate, sample_rate;
		switch (header.bitrate_index) {
			CASE_BIT_RATE(0b0000,	   0);
			CASE_BIT_RATE(0b0001,  32000);
			CASE_BIT_RATE(0b0010,  40000);
			CASE_BIT_RATE(0b0011,  48000);
			CASE_BIT_RATE(0b0100,  56000);
			CASE_BIT_RATE(0b0101,  64000);
			CASE_BIT_RATE(0b0110,  80000);
			CASE_BIT_RATE(0b0111,  96000);
			CASE_BIT_RATE(0b1000, 112000);
			CASE_BIT_RATE(0b1001, 128000);
			CASE_BIT_RATE(0b1010, 160000);
			CASE_BIT_RATE(0b1011, 192000);
			CASE_BIT_RATE(0b1100, 224000);
			CASE_BIT_RATE(0b1101, 256000);
			CASE_BIT_RATE(0b1110, 320000);
			CASE_BIT_RATE(0b1111,      0);
		}

		switch (header.freq_index) {
			CASE_SAMPLE_RATE(0b00, 44100);
			CASE_SAMPLE_RATE(0b01, 48000);
			CASE_SAMPLE_RATE(0b10, 32000);
			CASE_SAMPLE_RATE(0b11, 	   0);
		}

		return static_cast<uint32_t>(144 * ((float)bit_rate / (float)sample_rate) + header.padding);
	}

	Mp3Metadata Mp3Load(const std::string& str)
	{
		FILE* fp;
		if (fopen_s(&fp, str.c_str(), "rb") != 0) {
			return {};
		}
		
		//Copy the data into the buffer
		Mp3Metadata data;
		fseek(fp, 0, SEEK_END);
		uint32_t file_size = ftell(fp);
		data.filebuf = std::make_unique<uint8_t[]>(file_size);
		fseek(fp, 0, SEEK_SET);
		fread(data.filebuf.get(), 1, file_size, fp);
		fseek(fp, 0, SEEK_SET);

		uint32_t file_cursor = 0;
		char tag_mark[4] = {};
		uint8_t* filebuf = data.filebuf.get();
		int index = 0;
		while (!feof(fp)) {
			if (index == 9497) {
				int i = 0;
			}
			index++;
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
			frame.header = ReadMp3Header(fp);
			if (frame.header.frame_synchronizer != 0b11111111111 || frame.header.mpeg_ver_id != 3 || frame.header.layer != 1) {
				//Unusual Mp3 format, end the stream here
				data.frames.pop_back();
				return data;
			}

			uint32_t header_position = file_cursor;
			uint32_t frame_length = ComputeMp3FrameLength(frame.header);

			file_cursor += 4;

			if (frame.header.crc_protection == 0) {
				fread(&frame.crc_data, 2, 1, fp);
				file_cursor += 2;
			}

			//If non mono
			frame.channel_info.ptr = filebuf + file_cursor;
			if (frame.header.channel_type != 3) {
				frame.channel_info.size = 32;
				file_cursor += 32;
			}
			else {
				frame.channel_info.size = 17;
				file_cursor += 17;
			}

			frame.sound_data.ptr = filebuf + file_cursor;
			frame.sound_data.size = frame_length - (file_cursor - header_position);
			file_cursor = header_position + frame_length;
		}

		fclose(fp);
		return data;
	}

}

#undef CASE_BIT_RATE
#undef CASE_SAMPLE_RATE

