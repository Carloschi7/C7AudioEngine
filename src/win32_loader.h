#pragma once
#include <stdio.h>
#include <windows.h>
#include <dsound.h>
#include <stdint.h>
#include <math.h>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include "loader.h"

namespace Audio
{
	struct Win32BufferHandle {
		IDirectSoundBuffer* buffer;
		uint32_t sample_rate;
		uint32_t bytes_per_sample;
		uint32_t cursor;
		uint32_t volume;

		struct {
			uint8_t* data = nullptr;
			uint32_t size = 0;
		} file_payload;
	};
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

	typedef HRESULT WINAPI direct_sound_create(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUND* ppDS, _Pre_null_ LPUNKNOWN pUnkOuter);
	typedef HRESULT WINAPI direct_sound_enumerate(_In_ LPDSENUMCALLBACKA pDSEnumCallback, _In_opt_ LPVOID pContext);

	class Win32Mixer : public Mixer
	{
	public:
		Win32Mixer(HWND& window, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume);
		Win32Mixer(HWND& window, const std::string& wav_filename, uint32_t volume);
		~Win32Mixer();
		//TODO constructor
		virtual void UpdateRawWave(const WaveFunc& func) override;
		virtual void UpdateFileWav() override;
		virtual void AsyncPlay() override;
		virtual void Stop() override;
	private:
		void InitDirectsound(HWND& window, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume);
		void ParseHeader(WavHeader& header, FILE* fp);

	private:
		Win32BufferHandle m_BufferHandle;
	};
}