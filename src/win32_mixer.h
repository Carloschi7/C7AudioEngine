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
#include "mixer.h"

namespace Audio
{
	struct Win32FilePayload 
	{
		std::unique_ptr<uint8_t[]> data = nullptr;
		uint32_t size = 0;
	};

	struct Win32BufferHandle
	{
		IDirectSoundBuffer* buffer;
		uint32_t sample_rate;
		uint32_t channels;
		uint32_t bytes_per_sample;
		uint32_t cursor;
		uint32_t volume;
		Win32FilePayload file_payload;
		WaveFunc wavefunc;
	};


	typedef HRESULT WINAPI direct_sound_create(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUND* ppDS, _Pre_null_ LPUNKNOWN pUnkOuter);
	typedef HRESULT WINAPI direct_sound_enumerate(_In_ LPDSENUMCALLBACKA pDSEnumCallback, _In_opt_ LPVOID pContext);

	//Uses Directsound API atm, they say its obsolete, but it is pretty functional and compatible and straight-forward, so seems a fine choice
	class Win32Mixer : public Mixer
	{
	public:
		Win32Mixer();
		~Win32Mixer();
		//TODO constructor
		virtual void PushCustomWave(const WaveFunc& func, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume) override;
		virtual void PushAudioFile(const std::string& filename) override;
		virtual void UpdateCustomWave(const WaveFunc& func) override;
		virtual void UpdateFile() override;
		virtual void Wait() override;
		virtual void AsyncPlay() override;
		virtual void Stop() override;

	private:
		Win32BufferHandle* m_BufferHandle;
		//Windows API impl
		HWND m_Window;
	};
	
	static Win32BufferHandle* InitDirectsound(HWND& window, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume);
}