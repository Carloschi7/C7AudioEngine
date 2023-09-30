#pragma once
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace Audio 
{
	//Slim interface for now
	using WaveFunc = std::function<float(float)>;
	enum class MixerMode : uint8_t
	{
		RAW_WAVE_STREAM = 0,
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
		Mixer(MixerMode mode);
		virtual ~Mixer() {};
		virtual void UpdateRawWave(const WaveFunc& func) = 0;
		virtual void UpdateFileWav() = 0;
		virtual void AsyncPlay() = 0;
		virtual void Stop() = 0;
	protected:
		MixerMode m_LoaderMode;
		std::atomic<bool> m_AsyncPlayRunning = false;
		std::thread m_AsyncPlayThread;
	};

	class NullMixer : public Mixer
	{
	public:
		virtual void UpdateRawWave(const WaveFunc& func) override {};
		virtual void UpdateFileWav() override {};
		virtual void AsyncPlay() override {};
		virtual void Stop() override {};
	};

	template<class... Args>
	std::unique_ptr<Mixer> GenerateMixer(Args&&... init_args)
	{
		//TODO make this work also for other win distributions
#ifdef _MSC_VER
		return std::make_unique<Win32Mixer>(std::forward<Args>(init_args)...);
#elif defined __linux__ || defined UNIX
		return std::make_unique<LinuxMixer>(std::forward<Args>(init_args)...);
#else
		return std::make_unique<NullMixer>();
#endif
		//UNREACHABLE
		return nullptr;
	}
}