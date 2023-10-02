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

	//Header of supported formats
	struct WavHeader 
	{
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

	union Mp3Header 
	{
		struct {
			uint32_t emphasis : 2;
			uint32_t original : 1;
			uint32_t copyright : 1;
			uint32_t mode_extension : 2;
			uint32_t channel_type : 2;
			uint32_t private_bit : 1;
			uint32_t padding : 1;
			uint32_t freq_index : 2;
			uint32_t bitrate_index : 4;
			uint32_t crc_protection : 1;
			uint32_t layer : 2;
			uint32_t mpeg_ver_id : 2;
			uint32_t frame_synchronizer : 11;
		};
		uint32_t hex;
	};

	struct Mp3Section
	{
		uint8_t* ptr;
		uint32_t size;
	};

	struct Mp3Frame
	{
		Mp3Header header;
		uint16_t crc_data;
		Mp3Section channel_info;
		Mp3Section sound_data;
	};

	//no v1/v2 tags stored at the moment
	struct Mp3Metadata
	{
		std::unique_ptr<uint8_t[]> filebuf;
		std::vector<Mp3Frame> frames;
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

	//WAV functions
	WavHeader ReadWavHeader(FILE* fp);
	//MP3 functions
	Mp3Header ReadMp3Header(FILE* fp);
	uint32_t ComputeMp3FrameLength(Mp3Header header);
	Mp3Metadata Mp3Load(const std::string& str);

	template<typename type, std::enable_if_t<std::is_integral_v<type>, int> = 0>
	void SwapBytes(type& value)
	{
		uint8_t payload[sizeof(type)];
		std::memcpy(payload, &value, sizeof(type));

		value = 0;
		uint32_t type_bits = sizeof(type) * 8;
		for (uint32_t i = 0; i < sizeof(type); i++) {
			type_bits -= 8;
			value |= static_cast<type>(payload[i]) << type_bits;
		}
	}

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