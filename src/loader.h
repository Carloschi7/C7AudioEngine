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

	class NullLoader : public Mixer
	{
	public:
		virtual void UpdateRawWave(const WaveFunc& func) override {};
		virtual void UpdateFileWav() override {};
	};

	template<class... Args>
	std::unique_ptr<Mixer> GenerateLoader(Args&&... init_args)
	{
		//TODO make this work also for other win distributions
#ifdef _MSC_VER
		return std::make_unique<Win32Mixer>(std::forward<Args>(init_args)...);
#elif defined __linux__
		return std::make_unique<LinuxLoader>(std::forward<Args>(init_args)...);
#else
		return std::make_unique<NullLoader>();
#endif
		//UNREACHABLE
		return nullptr;
	}
}