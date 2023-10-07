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
	//Load only two chunks of file data in a pointer, so that we dont have
	//to store the whole file in a buffer
	static constexpr uint32_t g_MaxFilePortion = 2'000'000;
	struct Win32FilePayload 
	{
		FILE* fp;
		uint32_t file_size = 0;
		std::vector<uint8_t> loaded_chunk;
		uint32_t chunk_position;
		uint32_t swap_index;

		uint8_t* read_at(uint32_t pos) {
			//pos %= file_size;

			if (chunk_position + g_MaxFilePortion * 2 < file_size) {
				if (pos - chunk_position < g_MaxFilePortion * 2)
					return loaded_chunk.data() + (pos - chunk_position);
			}
			else {
				//Wraps at the beginning
				if (pos < chunk_position + g_MaxFilePortion * 2 - file_size)
					return loaded_chunk.data() + (file_size - chunk_position) + pos;

				if (pos < file_size)
					return loaded_chunk.data() + (pos - chunk_position);
			}
			//Portion of the file not loaded at the moment
			return nullptr;
		}

		void swap_buffers() {
			//Copy the forward part in the back part
			std::memcpy(loaded_chunk.data(), loaded_chunk.data() + g_MaxFilePortion, g_MaxFilePortion);
			chunk_position = (chunk_position + g_MaxFilePortion) % file_size;

			uint32_t seek_position = (chunk_position + g_MaxFilePortion) % file_size;
			uint8_t* second_ptr = loaded_chunk.data() + g_MaxFilePortion;
			if (seek_position + g_MaxFilePortion < file_size) {
				fseek(fp, seek_position, SEEK_SET);
				fread(second_ptr, 1, g_MaxFilePortion, fp);
			} 
			else {
				fseek(fp, seek_position, SEEK_SET);
				uint32_t newsize = file_size - seek_position;
				fread(second_ptr, 1, newsize, fp);
				fseek(fp, 0, SEEK_SET);
				fread(second_ptr + newsize, 1, g_MaxFilePortion - newsize, fp);
			}
		}

		~Win32FilePayload() {
			if (fp)
				fclose(fp);
		}
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