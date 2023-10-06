#include "linux_mixer.h"
#include "win32_mixer.h"

namespace Audio {

	void LinuxMixer::PushCustomWave(const WaveFunc& func, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume)
	{

	}

	void LinuxMixer::PushAudioFile(const std::string& filename)
	{
	}

	void LinuxMixer::UpdateCustomWave(const WaveFunc& func)
	{
	}

	void LinuxMixer::UpdateFile()
	{
	}

	void LinuxMixer::Wait()
	{
	}

	void LinuxMixer::AsyncPlay()
	{
	}

	void LinuxMixer::Stop()
	{
	}

}
