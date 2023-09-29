#pragma once
#include "loader.h"

namespace Audio 
{
	class LinuxMixer : public Mixer 
	{
	public:
		//TODO constructor
		virtual void UpdateRawWave(const WaveFunc& func) override;
		virtual void UpdateFileWav() override;
		virtual void AsyncPlay() override;
		virtual void Stop() override;
	};
}