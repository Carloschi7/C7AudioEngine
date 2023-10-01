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
}
