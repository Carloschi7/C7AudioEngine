#include "win32_mixer.h"

//Interface impl
static bool application_running = true;
static constexpr float pi = 3.14159265359f;

namespace Audio 
{
	static LRESULT CALLBACK WindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

	Win32Mixer::Win32Mixer() :
		Mixer{}, m_Window {}, m_BufferHandle{ nullptr }
	{
		//Init windows API
		const wchar_t class_name[] = L"Sample Window Class";

		WNDCLASS wc = {};

		//TODO find a way to remove hInstance, using it by assigning it to zero is not the
		//best practice
		HINSTANCE hInstance = 0;
		wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = class_name;

		if (!RegisterClass(&wc)) {
			return;
		}

		m_Window = CreateWindowEx(
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

		if (!m_Window) {
			//TODO error msg
			return;
		}
		
		//More of a process than an actual window, that is needed though to init directsound, so hiding
		//it in the background
		ShowWindow(m_Window, SW_HIDE);
	}

	Win32Mixer::~Win32Mixer()
	{
		if(m_AsyncPlayRunning)
			Stop();

		if (m_BufferHandle->file_payload.data)
			delete[] m_BufferHandle->file_payload.data;

		delete m_BufferHandle;

		//Windows API
		DestroyWindow(m_Window);
	}

	void Win32Mixer::PushCustomWave(const WaveFunc& func, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume)
	{
		if (m_MixerMode != MixerMode::NONE)
			return;

		m_MixerMode = MixerMode::RAW_WAVE_STREAM;
		//Simple init
		m_BufferHandle = InitDirectsound(m_Window, sample_rate, channels, bytes_per_channel, volume);
		if (!m_BufferHandle) {
			//TODO handle properly
			OutputDebugStringA("Windows mixer not initialized");
			return;
		}

		m_BufferHandle->wavefunc = func;
	}

	void Win32Mixer::PushAudioFile(const std::string& filename)
	{
		if (m_MixerMode != MixerMode::NONE)
			return;

		m_MixerMode = MixerMode::FILE_WAV_STREAM;

		FILE* fp;
		if (fopen_s(&fp, filename.c_str(), "rb") != 0)
		{
			//TODO handle error
			return;
		}

		//According to the standard WAV file composition
		WavHeader header;
		fread(&header, sizeof(WavHeader), 1, fp);
		ParseHeader(header, fp);
		m_BufferHandle = InitDirectsound(m_Window, header.sample_rate, header.channels, header.bits_per_sample / 8, 1000);
		if (!m_BufferHandle) {
			//TODO handle properly
			OutputDebugStringA("Windows mixer not initialized");
			return;
		}

		//Load file payload into mem
		uint32_t& file_size = m_BufferHandle->file_payload.size;
		file_size = header.data_size;
		m_BufferHandle->file_payload.data = new uint8_t[file_size];
		fread(m_BufferHandle->file_payload.data, 1, file_size, fp);
		fclose(fp);
	}

	void Win32Mixer::UpdateCustomWave(const WaveFunc& func)
	{
		IDirectSoundBuffer* sound_buffer = m_BufferHandle->buffer;
		uint32_t generator = m_BufferHandle->cursor / m_BufferHandle->bytes_per_sample;

		DWORD play_cur, write_cur;
		if (sound_buffer->GetCurrentPosition(&play_cur, &write_cur) < 0) {
			//TODO handle error
			return;
		}

		uint32_t byte_to_lock = m_BufferHandle->cursor % m_BufferHandle->sample_rate;
		uint32_t size = (play_cur < byte_to_lock) ?
			m_BufferHandle->sample_rate - (byte_to_lock - play_cur) :
			play_cur - byte_to_lock;

		void* audiobuf1, * audiobuf2;
		DWORD size1, size2;

		sound_buffer->Lock(byte_to_lock, size, &audiobuf1, &size1, &audiobuf2, &size2, 0);
		if (size1 != 0 || size2 != 0) {
			//Play sound
			int16_t* castedbuf1 = static_cast<int16_t*>(audiobuf1);
			int16_t* castedbuf2 = static_cast<int16_t*>(audiobuf2);

			//Tone settings
			uint32_t sample_size1 = size1 / m_BufferHandle->bytes_per_sample;
			uint32_t sample_size2 = size2 / m_BufferHandle->bytes_per_sample;

			for (uint32_t i = 0; i < sample_size1; i++) {
				float fval = func((float)generator++);
				int16_t val = int16_t(fval * (float)m_BufferHandle->volume);
				*castedbuf1++ = val;
				*castedbuf1++ = val;
			}

			for (uint32_t i = 0; i < sample_size2; i++) {
				float fval = func((float)generator++);
				int16_t val = int16_t(fval * (float)m_BufferHandle->volume);
				*castedbuf2++ = val;
				*castedbuf2++ = val;
			}

			m_BufferHandle->cursor += (size1 + size2);
		}
		sound_buffer->Unlock(audiobuf1, size1, audiobuf2, size2);
	}

	void Win32Mixer::UpdateFileWav()
	{
		IDirectSoundBuffer* sound_buffer = m_BufferHandle->buffer;
		uint32_t generator = m_BufferHandle->cursor / m_BufferHandle->bytes_per_sample;

		DWORD play_cur, write_cur;
		if (sound_buffer->GetCurrentPosition(&play_cur, &write_cur) < 0) {
			//TODO handle error
			return;
		}

		uint32_t byte_to_lock = m_BufferHandle->cursor % m_BufferHandle->sample_rate;
		uint32_t size = (play_cur < byte_to_lock) ?
			m_BufferHandle->sample_rate - (byte_to_lock - play_cur) :
			play_cur - byte_to_lock;

		void* audiobuf1, * audiobuf2;
		DWORD size1, size2;

		sound_buffer->Lock(byte_to_lock, size, &audiobuf1, &size1, &audiobuf2, &size2, 0);
		uint8_t* filebuf = m_BufferHandle->file_payload.data;
		if (size1 != 0 || size2 != 0) {
			uint8_t* casted_buf1 = static_cast<uint8_t*>(audiobuf1);
			uint8_t* casted_buf2 = static_cast<uint8_t*>(audiobuf2);

			uint32_t getpoint = m_BufferHandle->cursor % m_BufferHandle->file_payload.size;
			if (getpoint + size1 < m_BufferHandle->file_payload.size) {
				std::memcpy(casted_buf1, filebuf + getpoint, size1);
			}
			else {
				uint32_t half_copy_size = m_BufferHandle->file_payload.size - getpoint;
				std::memcpy(casted_buf1, filebuf + getpoint, half_copy_size);
				std::memcpy(casted_buf1 + half_copy_size, filebuf, size1 - half_copy_size);
			}

			if (size2 != 0) {
				getpoint = (m_BufferHandle->cursor + size1) % m_BufferHandle->file_payload.size;
				if (getpoint + size2 < m_BufferHandle->file_payload.size) {
					std::memcpy(casted_buf2, filebuf + getpoint, size2);
				}
				else {
					uint32_t half_copy_size = m_BufferHandle->file_payload.size - getpoint;
					std::memcpy(casted_buf2, filebuf + getpoint, half_copy_size);
					std::memcpy(casted_buf2 + half_copy_size, filebuf, size2 - half_copy_size);
				}
			}

			m_BufferHandle->cursor += (size1 + size2);
		}
		sound_buffer->Unlock(audiobuf1, size1, audiobuf2, size2);
	}

	void Win32Mixer::Wait()
	{
		MSG msg;
		BOOL bRet;
		while (application_running) {
			if ((bRet = GetMessage(&msg, m_Window, 0, 0)) != 0)
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

	void Win32Mixer::AsyncPlay()
	{
		//Outputs a default A freq at the moment
		m_AsyncPlayRunning = true;
		m_AsyncPlayThread = std::thread([this]() {
			while (m_AsyncPlayRunning) {
				switch (m_MixerMode) 
				{
				case MixerMode::RAW_WAVE_STREAM:
					UpdateCustomWave(m_BufferHandle->wavefunc);
				case MixerMode::FILE_WAV_STREAM:
					UpdateFileWav();
				}
			}
		});
		m_BufferHandle->buffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	void Win32Mixer::Stop()
	{
		m_AsyncPlayRunning = false;
		m_AsyncPlayThread.join();
		m_BufferHandle->buffer->Stop();
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

	static Win32BufferHandle* InitDirectsound(HWND& window, uint32_t sample_rate, uint32_t channels, uint32_t bytes_per_channel, uint32_t volume)
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
						Win32BufferHandle* buffer_handle = new Win32BufferHandle;
						buffer_handle->buffer = secondary_buffer;
						buffer_handle->sample_rate = sample_rate;
						buffer_handle->bytes_per_sample = bytes_per_sample;
						buffer_handle->cursor = 0;
						buffer_handle->volume = volume;
						return buffer_handle;
					}
				}

			}
		}
		else {
			//TODO(carlo) log error
		}

		return nullptr;
	}
}

#define PATH(path) std::string(ROOT_DIR) + '/' + std::string(path)
#define CPATH(path) (std::string(ROOT_DIR) + '/' + std::string(path)).c_str()

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) 
{
	std::unique_ptr<Audio::Mixer> buffer_handle = Audio::GenerateMixer();
	if (buffer_handle == nullptr) {
		OutputDebugStringA("Audio could not be initialized");
		return -1;
	}

	buffer_handle->PushAudioFile(PATH("assets/sound_samples/ballin.wav"));
	buffer_handle->AsyncPlay();
	buffer_handle->Wait();

	return 0;
}