#pragma once
#include "mixer.h"

namespace Audio 
{
	class LinuxMixer : public Mixer 
	{
	public:
		//TODO constructor
		virtual void PushCustomWave(const WaveFunc& func, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume) override;
		virtual void PushAudioFile(const std::string& filename) override;
		virtual void UpdateCustomWave(const WaveFunc& func) override;
		virtual void UpdateFile() override;
		virtual void Wait() override;
		virtual void AsyncPlay() override;
		virtual void Stop() override;
	};
}