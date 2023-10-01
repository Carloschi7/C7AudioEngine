#pragma once
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <string>

namespace Audio 
{
	//Slim interface for now
	using WaveFunc = std::function<float(float)>;
	enum class MixerMode : uint8_t
	{
		NONE = 0,
		RAW_WAVE_STREAM,
		FILE_WAV_STREAM,
	};

	//Header of supported formats
	struct WavHeader {
		uint8_t riff_str[4];
		uint32_t file_size;
		uint8_t wave_str[4];
		uint8_t fmt_str[4];
		uint32_t format_data;
		uint16_t format_type;
		uint16_t channels;
		uint32_t sample_rate;
		uint32_t byte_rate;
		uint16_t unused;
		uint16_t bits_per_sample;
		uint8_t data_str[4];
		uint32_t data_size;
	};

	class Mixer
	{
	public:
		Mixer();
		virtual ~Mixer();
		virtual void PushCustomWave(const WaveFunc& func, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume) = 0;
		virtual void PushAudioFile(const std::string& filename) = 0;
		virtual void UpdateCustomWave(const WaveFunc& func) = 0;
		virtual void UpdateFileWav() = 0;

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
		virtual void UpdateFileWav() override {}
		virtual void Wait() override {}
		virtual void AsyncPlay() override {}
		virtual void Stop() override {}
	};

	std::unique_ptr<Mixer> GenerateMixer();
}