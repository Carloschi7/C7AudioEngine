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


}
