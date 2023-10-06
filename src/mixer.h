#pragma once
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

namespace Audio 
{
	//Slim interface for now
	using WaveFunc = std::function<float(float)>;
	enum class MixerMode : uint8_t
	{
		NONE = 0,
		RAW_WAVE_STREAM,
		FILE_WAV_STREAM,
		FILE_MP3_STREAM,
	};





	class Mixer
	{
	public:
		Mixer();
		virtual ~Mixer();
		virtual void PushCustomWave(const WaveFunc& func, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume) = 0;
		virtual void PushAudioFile(const std::string& filename) = 0;
		virtual void UpdateCustomWave(const WaveFunc& func) = 0;
		virtual void UpdateFile() = 0;

		virtual void Wait() = 0;
		virtual void AsyncPlay() = 0;
		virtual void Stop() = 0;
	protected:
		MixerMode m_MixerMode;
		std::atomic<bool> m_AsyncPlayRunning = false;
		std::thread m_AsyncPlayThread;
	};

	class NullMixer : public Mixer
	{
	public:
		virtual void PushCustomWave(const WaveFunc& func, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume) override {}
		virtual void PushAudioFile(const std::string& filename) override {}
		virtual void UpdateCustomWave(const WaveFunc& func) override {}
		virtual void UpdateFile() override {}
		virtual void Wait() override {}
		virtual void AsyncPlay() override {}
		virtual void Stop() override {}
	};

	std::unique_ptr<Mixer> GenerateMixer();






	template<typename type, uint32_t channels>
	void WriteWave(const WaveFunc& func, void* buffer, uint32_t size, uint32_t bytes_per_sample, uint32_t volume, uint32_t& generator)
	{
		type* castedbuf = static_cast<type*>(buffer);
		//Tone settings
		uint32_t sample_size = size / bytes_per_sample;

		for (uint32_t i = 0; i < sample_size; i++) {
			float fval = func((float)generator++);
			int16_t val = int16_t(fval * (float)volume);

			for (uint32_t j = 0; j < channels; j++)
				*castedbuf++ = val;
		}
	}
}