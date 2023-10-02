#include "mixer.h"
#include "win32_mixer.h"
#include "linux_mixer.h"

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
		char tag_mark[4] = {};
		fread(tag_mark, 1, 3, fp);
		uint32_t file_cursor = 0;
		while (std::strcmp(tag_mark, "TAG") == 0 || std::strcmp(tag_mark, "ID3") == 0) {
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
			fseek(fp, file_cursor, SEEK_SET);
			fread(tag_mark, 1, 3, fp);
		}
		fseek(fp, file_cursor, SEEK_SET);
		fread(&header, sizeof(Mp3Header), 1, fp);
		SwapBytes(header.hex);

		return header;
	}


}
