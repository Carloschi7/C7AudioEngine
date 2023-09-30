#include "win32_mixer.h"

//Interface impl
static bool application_running = true;
static constexpr float pi = 3.14159265359f;

namespace Audio 
{
	Win32Mixer::Win32Mixer(HWND& window, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume)
		: Mixer{ MixerMode::RAW_WAVE_STREAM }
	{
		//Simple init
		InitDirectsound(window, sample_rate, channels, bytes_per_channel, volume);
	}

	Win32Mixer::Win32Mixer(HWND& window, const std::string& wav_filename, uint32_t volume)
		: Mixer{ MixerMode::FILE_WAV_STREAM }
	{
		FILE* fp;
		if (fopen_s(&fp, wav_filename.c_str(), "rb") != 0)
		{
			//TODO handle error
			return;
		}

		//According to the standard WAV file composition
		WavHeader header;
		fread(&header, sizeof(WavHeader), 1, fp);
		ParseHeader(header, fp);
		InitDirectsound(window, header.sample_rate, header.channels, header.bits_per_sample / 8, volume);

		//Load file payload into mem
		uint32_t& file_size = m_BufferHandle.file_payload.size;
		file_size = header.data_size;
		m_BufferHandle.file_payload.data = new uint8_t[file_size];
		fread(m_BufferHandle.file_payload.data, 1, file_size, fp);
		fclose(fp);
	}

	Win32Mixer::~Win32Mixer()
	{
		if(m_AsyncPlayRunning)
			Stop();

		if (m_BufferHandle.file_payload.data)
			delete[] m_BufferHandle.file_payload.data;
	}

	void Win32Mixer::UpdateRawWave(const WaveFunc& func)
	{
		IDirectSoundBuffer* sound_buffer = m_BufferHandle.buffer;
		uint32_t generator = m_BufferHandle.cursor / m_BufferHandle.bytes_per_sample;

		DWORD play_cur, write_cur;
		if (sound_buffer->GetCurrentPosition(&play_cur, &write_cur) < 0) {
			//TODO handle error
			return;
		}

		uint32_t byte_to_lock = m_BufferHandle.cursor % m_BufferHandle.sample_rate;
		uint32_t size = (play_cur < byte_to_lock) ?
			m_BufferHandle.sample_rate - (byte_to_lock - play_cur) :
			play_cur - byte_to_lock;

		void* audiobuf1, * audiobuf2;
		DWORD size1, size2;

		sound_buffer->Lock(byte_to_lock, size, &audiobuf1, &size1, &audiobuf2, &size2, 0);
		if (size1 != 0 || size2 != 0) {
			//Play sound
			int16_t* castedbuf1 = static_cast<int16_t*>(audiobuf1);
			int16_t* castedbuf2 = static_cast<int16_t*>(audiobuf2);

			//Tone settings
			uint32_t sample_size1 = size1 / m_BufferHandle.bytes_per_sample;
			uint32_t sample_size2 = size2 / m_BufferHandle.bytes_per_sample;

			for (uint32_t i = 0; i < sample_size1; i++) {
				float fval = func((float)generator++);
				int16_t val = int16_t(fval * (float)m_BufferHandle.volume);
				*castedbuf1++ = val;
				*castedbuf1++ = val;
			}

			for (uint32_t i = 0; i < sample_size2; i++) {
				float fval = func((float)generator++);
				int16_t val = int16_t(fval * (float)m_BufferHandle.volume);
				*castedbuf2++ = val;
				*castedbuf2++ = val;
			}

			m_BufferHandle.cursor += (size1 + size2);
		}
		sound_buffer->Unlock(audiobuf1, size1, audiobuf2, size2);
	}

	void Win32Mixer::UpdateFileWav()
	{
		IDirectSoundBuffer* sound_buffer = m_BufferHandle.buffer;
		uint32_t generator = m_BufferHandle.cursor / m_BufferHandle.bytes_per_sample;

		DWORD play_cur, write_cur;
		if (sound_buffer->GetCurrentPosition(&play_cur, &write_cur) < 0) {
			//TODO handle error
			return;
		}

		uint32_t byte_to_lock = m_BufferHandle.cursor % m_BufferHandle.sample_rate;
		uint32_t size = (play_cur < byte_to_lock) ?
			m_BufferHandle.sample_rate - (byte_to_lock - play_cur) :
			play_cur - byte_to_lock;

		void* audiobuf1, * audiobuf2;
		DWORD size1, size2;

		sound_buffer->Lock(byte_to_lock, size, &audiobuf1, &size1, &audiobuf2, &size2, 0);
		uint8_t* filebuf = m_BufferHandle.file_payload.data;
		if (size1 != 0 || size2 != 0) {
			uint8_t* casted_buf1 = static_cast<uint8_t*>(audiobuf1);
			uint8_t* casted_buf2 = static_cast<uint8_t*>(audiobuf2);

			uint32_t getpoint = m_BufferHandle.cursor % m_BufferHandle.file_payload.size;
			if (getpoint + size1 < m_BufferHandle.file_payload.size) {
				std::memcpy(casted_buf1, filebuf + getpoint, size1);
			}
			else {
				uint32_t half_copy_size = m_BufferHandle.file_payload.size - getpoint;
				std::memcpy(casted_buf1, filebuf + getpoint, half_copy_size);
				std::memcpy(casted_buf1 + half_copy_size, filebuf, size1 - half_copy_size);
			}

			if (size2 != 0) {
				getpoint = (m_BufferHandle.cursor + size1) % m_BufferHandle.file_payload.size;
				if (getpoint + size2 < m_BufferHandle.file_payload.size) {
					std::memcpy(casted_buf2, filebuf + getpoint, size2);
				}
				else {
					uint32_t half_copy_size = m_BufferHandle.file_payload.size - getpoint;
					std::memcpy(casted_buf2, filebuf + getpoint, half_copy_size);
					std::memcpy(casted_buf2 + half_copy_size, filebuf, size2 - half_copy_size);
				}
			}

			m_BufferHandle.cursor += (size1 + size2);
		}
		sound_buffer->Unlock(audiobuf1, size1, audiobuf2, size2);
	}

	void Win32Mixer::AsyncPlay()
	{
		//Outputs a default A freq at the moment
		WaveFunc func = [&](float x) {
			return sin(x * (440.f / (float)(m_BufferHandle.sample_rate * m_BufferHandle.bytes_per_sample * 0.5f)) * pi);
		};

		m_AsyncPlayRunning = true;
		m_AsyncPlayThread = std::thread([this, func]() {
			while (m_AsyncPlayRunning) {
				switch (m_LoaderMode) 
				{
				case MixerMode::RAW_WAVE_STREAM:
					UpdateRawWave(func);
				case MixerMode::FILE_WAV_STREAM:
					UpdateFileWav();
				}
			}
		});
		m_BufferHandle.buffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	void Win32Mixer::Stop()
	{
		m_AsyncPlayRunning = false;
		m_AsyncPlayThread.join();
		m_BufferHandle.buffer->Stop();
	}

	void Win32Mixer::InitDirectsound(HWND& window, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume)
	{
		HMODULE module_directsound = LoadLibraryA("dsound.dll");
		if (module_directsound) {
			auto DirectSoundCreate = (direct_sound_create*)GetProcAddress(module_directsound, "DirectSoundCreate");

			if (DirectSoundCreate) {
				//DirectSoundEnumerate_(SoundEnumerate, nullptr);
				uint32_t bytes_per_sample = channels * bytes_per_channel;
				IDirectSound* direct_sound;

				WAVEFORMATEX wave_format = {};
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
					buffer_desc2.dwFlags = DSBCAPS_GLOBALFOCUS;
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
						OutputDebugStringA("Secondary buffer created\n");
						m_BufferHandle.buffer = secondary_buffer;
						m_BufferHandle.sample_rate = sample_rate;
						m_BufferHandle.bytes_per_sample = bytes_per_sample;
						m_BufferHandle.cursor = 0;
						m_BufferHandle.volume = volume;
					}
				}

			}
		}
		else {
			//TODO(carlo) log error
		}
	}

	void Win32Mixer::ParseHeader(WavHeader& header, FILE* fp)
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
}


#define PATH(path) std::string(ROOT_DIR) + '/' + std::string(path)
#define CPATH(path) (std::string(ROOT_DIR) + '/' + std::string(path)).c_str()


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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) 
{
	const wchar_t class_name[] = L"Sample Window Class";

	WNDCLASS wc = { };

	wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = class_name;

	if (!RegisterClass(&wc)) {
		return 0;
	}

	HWND hwnd = CreateWindowEx(
		0,
		class_name,
		L"Learn to Program Windows",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd) {
		ShowWindow(hwnd, SW_HIDE);

		uint32_t sample_size = 48'000, channels = 2, bytes_per_channel = 2, volume = 1000;
		uint32_t bytes_per_sample = channels * bytes_per_channel;

		float output_hz = 432.f;

		std::unique_ptr<Audio::Mixer> buffer_handle = Audio::GenerateMixer(hwnd, CPATH("assets/sound_samples/ballin.wav"), 1000);
		if (buffer_handle == nullptr) {
			OutputDebugStringA("Audio could not be initialized");
			return -1;
		}

		buffer_handle->AsyncPlay();
		MSG msg;
		BOOL bRet;
		while (application_running) {
			if ((bRet = GetMessage(&msg, hwnd, 0, 0)) != 0)
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
	}

	return 0;
}