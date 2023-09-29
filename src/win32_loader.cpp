#include <stdio.h>
#include <windows.h>
#include <dsound.h>
#include <stdint.h>
#include <math.h>
#include <functional>
#include <string>
#include <thread>
#include <atomic>

#define PATH(path) std::string(ROOT_DIR) + '/' + std::string(path)
#define CPATH(path) (std::string(ROOT_DIR) + '/' + std::string(path)).c_str()

static bool application_running = true;
static constexpr float pi = 3.14159265359f;

LRESULT CALLBACK WindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	switch (msg) {
	case WM_SIZE: {
		//WM_SIZE TODO
	} break;
	case WM_CLOSE: {
		application_running = false;
	}break;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(wnd, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)WHITENESS);
		EndPaint(wnd, &ps);
	}break;
	default:
		result = DefWindowProc(wnd, msg, wParam, lParam);
	}

	return result;
}

typedef HRESULT WINAPI direct_sound_create(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUND* ppDS, _Pre_null_ LPUNKNOWN pUnkOuter);
typedef HRESULT WINAPI direct_sound_enumerate(_In_ LPDSENUMCALLBACKA pDSEnumCallback, _In_opt_ LPVOID pContext);


BOOL CALLBACK SoundEnumerate(LPGUID lpGUID, LPCSTR lpszDesc, LPCSTR lpszDrvName, LPVOID lpContext)
{
	printf("lpContext = %s\n", (char*)lpContext);
	printf("Device description = %s\n", lpszDesc);
	printf("Driver name = %s\n", lpszDrvName);
	printf("\n");

	return TRUE;
}

namespace Audio 
{
	//Ingrained in the namespace
	struct BufferHandle {
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
	typedef WAVEFORMATEX WaveFormat;
	typedef std::function<float(float)> WaveFunc;

	BufferHandle* Init(HWND& window, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume)
	{
		HMODULE module_directsound = LoadLibraryA("dsound.dll");
		if (module_directsound) {
			auto DirectSoundCreate = (direct_sound_create*)GetProcAddress(module_directsound, "DirectSoundCreate");
			auto DirectSoundEnumerate_ = (direct_sound_enumerate*)GetProcAddress(module_directsound, "DirectSoundEnumerateA");

			if (DirectSoundEnumerate_ && DirectSoundCreate) {
				DirectSoundEnumerate_(SoundEnumerate, nullptr);
				uint32_t bytes_per_sample = channels * bytes_per_channel;
				IDirectSound* direct_sound;

				WaveFormat wave_format = {};
				wave_format.wFormatTag = WAVE_FORMAT_PCM;
				wave_format.nChannels = channels;
				wave_format.nSamplesPerSec = sample_rate;
				wave_format.wBitsPerSample = bytes_per_channel * 8;
				wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
				wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
				wave_format.cbSize = 0;

				bool init_res = DirectSoundCreate(0, &direct_sound, 0) >= 0;
				bool setlevel_res = direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY) >= 0;

				if (init_res && setlevel_res) {
					DSBUFFERDESC buffer_desc1 = {}, buffer_desc2 = {};
					//ZeroMemory(&buffer_desc1, sizeof(DSBUFFERDESC));
					buffer_desc1.dwSize = sizeof(buffer_desc1);
					buffer_desc1.dwFlags = DSBCAPS_PRIMARYBUFFER;

					buffer_desc2.dwSize = sizeof(buffer_desc2);
					buffer_desc2.dwFlags = 0;
					buffer_desc2.dwBufferBytes = sample_rate;
					buffer_desc2.lpwfxFormat = &wave_format;


					IDirectSoundBuffer* primary_buffer;
					if (direct_sound->CreateSoundBuffer(&buffer_desc1, &primary_buffer, nullptr) >= 0) {
						if (primary_buffer->SetFormat(&wave_format) >= 0) {
							OutputDebugStringA("Primary buffer created\n");
						}
					}

					IDirectSoundBuffer* secondary_buffer;
					if (direct_sound->CreateSoundBuffer(&buffer_desc2, &secondary_buffer, nullptr) >= 0) {
						OutputDebugStringA("Secondary buffer created");
						BufferHandle* handle = new BufferHandle;
						handle->buffer = secondary_buffer;
						handle->sample_rate = sample_rate;
						handle->bytes_per_sample = bytes_per_sample;
						handle->cursor = 0;
						handle->volume = volume;
						return handle;
					}
				}
				
			}
		}
		else {
			//TODO(carlo) log error
		}

		return nullptr;
	}

	void WriteWave(BufferHandle* buffer_handle, const WaveFunc& func) 
	{
		IDirectSoundBuffer* sound_buffer = buffer_handle->buffer;
		uint32_t generator = buffer_handle->cursor / buffer_handle->bytes_per_sample;

		DWORD play_cur, write_cur;
		if (sound_buffer->GetCurrentPosition(&play_cur, &write_cur) < 0) {
			//TODO handle error
			return;
		}

		uint32_t byte_to_lock = buffer_handle->cursor % buffer_handle->sample_rate;
		uint32_t size = (play_cur < byte_to_lock) ?
			buffer_handle->sample_rate - (byte_to_lock - play_cur) :
			play_cur - byte_to_lock;

		void* audiobuf1, * audiobuf2;
		DWORD size1, size2;

		sound_buffer->Lock(byte_to_lock, size, &audiobuf1, &size1, &audiobuf2, &size2, 0);
		if (size1 != 0 || size2 != 0) {
			//Play sound
			int16_t* castedbuf1 = static_cast<int16_t*>(audiobuf1);
			int16_t* castedbuf2 = static_cast<int16_t*>(audiobuf2);

			//Tone settings
			uint32_t sample_size1 = size1 / buffer_handle->bytes_per_sample;
			uint32_t sample_size2 = size2 / buffer_handle->bytes_per_sample;

			for (uint32_t i = 0; i < sample_size1; i++) {
				float fval = func((float)generator++);
				int16_t val = int16_t(fval * (float)buffer_handle->volume);
				*castedbuf1++ = val;
				*castedbuf1++ = val;
			}

			for (uint32_t i = 0; i < sample_size2; i++) {
				float fval = func((float)generator++);
				int16_t val = int16_t(fval * (float)buffer_handle->volume);
				*castedbuf2++ = val;
				*castedbuf2++ = val;
			}

			buffer_handle->cursor += (size1 + size2);
		}
		sound_buffer->Unlock(audiobuf1, size1, audiobuf2, size2);
	}

	namespace WAV {

		struct Header {
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

		//Exclude all non-data RIFF tags at the moment
		void ParseHeader(Header& header, FILE* fp) 
		{
			if (std::strncmp(reinterpret_cast<const char*>(header.data_str), "data", 4) == 0)
				return;

			//First tag offset + size
			uint32_t offset = 36 + 4 + 4;
			while (std::strncmp(reinterpret_cast<const char*>(header.data_str), "data", 4) != 0) {
				offset += header.data_size;
				fseek(fp, offset, SEEK_SET);
				fread(&header.data_str, 1, 4, fp);
				fread(&header.data_size, 4, 1, fp);
			}
		}

		BufferHandle* Init(HWND& window, const char* wav_filename, uint32_t volume)
		{
			FILE* fp;
			if (fopen_s(&fp, wav_filename, "rb") != 0)
			{
				//TODO handle error
				return nullptr;
			}

			//According to the standard WAV file composition
			Header header;
			fread(&header, sizeof(Header), 1, fp);
			ParseHeader(header, fp);
			BufferHandle* buf_handle = Audio::Init(window, header.sample_rate, header.channels, header.bits_per_sample / 8, volume);

			//Load file payload into mem
			uint32_t& file_size = buf_handle->file_payload.size;
			file_size = header.data_size;
			buf_handle->file_payload.data = new uint8_t[file_size];
			fread(buf_handle->file_payload.data, 1, file_size, fp);
			fclose(fp);

			return buf_handle;
		}

		void WriteWave(BufferHandle* buffer_handle)
		{
			/*auto func = [buffer_handle](float x) {
				uint32_t modded = (uint32_t)x % buffer_handle->file_payload.size;
				uint8_t value = buffer_handle->file_payload.data[modded];
				return (float)value / (float)0xff;
			};
			Audio::WriteWave(buffer_handle, func);*/

			IDirectSoundBuffer* sound_buffer = buffer_handle->buffer;
			uint32_t generator = buffer_handle->cursor / buffer_handle->bytes_per_sample;

			DWORD play_cur, write_cur;
			if (sound_buffer->GetCurrentPosition(&play_cur, &write_cur) < 0) {
				//TODO handle error
				return;
			}

			uint32_t byte_to_lock = buffer_handle->cursor % buffer_handle->sample_rate;
			uint32_t size = (play_cur < byte_to_lock) ?
				buffer_handle->sample_rate - (byte_to_lock - play_cur) :
				play_cur - byte_to_lock;

			void* audiobuf1, * audiobuf2;
			DWORD size1, size2;

			sound_buffer->Lock(byte_to_lock, size, &audiobuf1, &size1, &audiobuf2, &size2, 0);
			uint8_t* filebuf = buffer_handle->file_payload.data;
			if (size1 != 0 || size2 != 0) {
				uint8_t* casted_buf1 = static_cast<uint8_t*>(audiobuf1);
				uint8_t* casted_buf2 = static_cast<uint8_t*>(audiobuf2);

				uint32_t getpoint = buffer_handle->cursor % buffer_handle->file_payload.size;
				if (getpoint + size1 < buffer_handle->file_payload.size) {
					std::memcpy(casted_buf1, filebuf + getpoint, size1);
				}
				else {
					uint32_t half_copy_size = buffer_handle->file_payload.size - getpoint;
					std::memcpy(casted_buf1, filebuf + getpoint, half_copy_size);
					std::memcpy(casted_buf1 + half_copy_size, filebuf, size1 - half_copy_size);
				}

				if (size2 != 0) {
					getpoint = (buffer_handle->cursor + size1) % buffer_handle->file_payload.size;
					if (getpoint + size2 < buffer_handle->file_payload.size) {
						std::memcpy(casted_buf2, filebuf + getpoint, size2);
					}
					else {
						uint32_t half_copy_size = buffer_handle->file_payload.size - getpoint;
						std::memcpy(casted_buf2, filebuf + getpoint, half_copy_size);
						std::memcpy(casted_buf2 + half_copy_size, filebuf, size2 - half_copy_size);
					}
				}

				buffer_handle->cursor += (size1 + size2);
			}
			sound_buffer->Unlock(audiobuf1, size1, audiobuf2, size2);
		}
	}

	void Play(BufferHandle* buffer_handle) 
	{
		buffer_handle->buffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	void Stop(BufferHandle* buffer_handle)
	{
		buffer_handle->buffer->Stop();
	}

	void Terminate(BufferHandle*& buffer_handle)
	{
		if (buffer_handle->file_payload.data)
			delete[] buffer_handle->file_payload.data;

		delete buffer_handle;
		buffer_handle = nullptr;
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) 
{
	const wchar_t CLASS_NAME[] = L"Sample Window Class";

	WNDCLASS wc = { };

	wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	if (!RegisterClass(&wc)) {
		return 0;
	}

	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Learn to Program Windows",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd) {
		ShowWindow(hwnd, nCmdShow);

		uint32_t sample_size = 48'000, channels = 2, bytes_per_channel = 2, volume = 1000;
		uint32_t bytes_per_sample = channels * bytes_per_channel;

		float output_hz = 432.f;

		//Audio::BufferHandle* buffer_handle = Audio::Init(hwnd, sample_size, channels, bytes_per_channel, volume);
		Audio::BufferHandle* buffer_handle = Audio::WAV::Init(hwnd, CPATH("assets/sound_samples/ballin.wav"), volume);
		if (buffer_handle == nullptr) {
			OutputDebugStringA("Audio could not be initialized");
			return -1;
		}

		//Audio
		Audio::WaveFunc func = [&](float x) {
			return sin(x * (output_hz / (float)(sample_size * bytes_per_sample * 0.5f)) * pi);
		};

		Audio::WaveFunc square_func = [&](float x) {
			return sin(x * (output_hz / (float)(sample_size * bytes_per_sample * 0.5f)) * pi) > 0.0f ? 1.0f : -1.0f;
		};

		Audio::WaveFunc smooth_chainsaw_func = [&](float x) {
			float p = fmod(x * (output_hz / (float)(sample_size * bytes_per_sample * 0.5f)), pi);
			return cos(p * pi);
		};

		//Message handling
		MSG msg;
		BOOL bRet;
		//Update sound in a separate thread
		std::atomic_bool sound_thread_condition = true;
		std::thread sound_thread([&]() {
			//Middle C
			while (sound_thread_condition) {
				//output_hz *= 1.0000001f;
				//Audio::WriteWave(buffer_handle, func);
				Audio::WAV::WriteWave(buffer_handle);
			}
		});

		Audio::Play(buffer_handle);
		while (application_running) {
			if ((bRet = PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) != 0)
			{
				if (bRet == -1)
				{
					// handle the error and possibly exit
					break;
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		sound_thread_condition = false;
		sound_thread.join();

		Audio::Stop(buffer_handle);
		Audio::Terminate(buffer_handle);
	}

	return 0;
}