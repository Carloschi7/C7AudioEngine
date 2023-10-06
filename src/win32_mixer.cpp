#include "win32_mixer.h"
#include "setup.h"
#include "wav.h"
#include "mp3.h"

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

		if (m_BufferHandle) 
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
			LOG_ERROR("Windows mixer not initialized");
			return;
		}

		m_BufferHandle->wavefunc = func;
	}

	void Win32Mixer::PushAudioFile(const std::string& filename)
	{
		if (m_MixerMode != MixerMode::NONE)
			return;
		
		FILE* fp;
		if (fopen_s(&fp, filename.c_str(), "rb") != 0)
		{
			//TODO handle error
			return;
		}

		std::string extension = filename.substr(filename.find_last_of('.'));
		if (extension == ".wav") {
			m_MixerMode = MixerMode::FILE_WAV_STREAM;

			//According to the standard WAV file composition
			WAV::Header header = WAV::ReadHeader(fp);
			m_BufferHandle = InitDirectsound(m_Window, header.sample_rate, header.channels, header.bits_per_sample / 8, 1000);
			if (!m_BufferHandle) {
				//TODO handle properly
				LOG_ERROR("Windows mixer not initialized");
				return;
			}

			//Load file payload into mem
			uint32_t& file_size = m_BufferHandle->file_payload.size;
			file_size = header.data_size;
			m_BufferHandle->file_payload.data = std::make_unique<uint8_t[]>(file_size);
			fread(m_BufferHandle->file_payload.data.get(), 1, file_size, fp);
			fclose(fp);
		}
		else if (extension == ".mp3") {
			m_MixerMode = MixerMode::FILE_MP3_STREAM;
			//TODO implement
			MP3::Metadata data = MP3::Load(filename);
			uint32_t sample_rate, channels, bits_per_sample;
			MP3::ExtractDirectsoundData(data.first_header, sample_rate, channels, bits_per_sample);
			m_BufferHandle = InitDirectsound(m_Window, sample_rate, channels, bits_per_sample / 8, 1000);
			if (!m_BufferHandle) {
				//TODO handle properly
				LOG_ERROR("Windows mixer not initialized");
				return;
			}

			std::unique_ptr<int16_t[]> output = std::make_unique<int16_t[]>(data.max_pcm_samples);
			MP3::Decode(data, output.get(), data.max_pcm_samples);
			//Bind it as an uint8_t ptr to the buffer handle
			m_BufferHandle->file_payload.data.swap(std::unique_ptr<uint8_t[]>{ reinterpret_cast<uint8_t*>(output.release()) });
			m_BufferHandle->file_payload.size = data.max_pcm_samples * sizeof(int16_t);
		}
		else {
			LOG_ERROR("Format not supported");
		}
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
			uint32_t volume = m_BufferHandle->volume;
			uint32_t bytes_per_sample = m_BufferHandle->bytes_per_sample;
			if (m_BufferHandle->channels == 2) {
				WriteWave<int16_t, 2>(m_BufferHandle->wavefunc, audiobuf1, size1, bytes_per_sample, volume, generator);
				WriteWave<int16_t, 2>(m_BufferHandle->wavefunc, audiobuf2, size2, bytes_per_sample, volume, generator);
			}
			else {
				WriteWave<int16_t, 1>(m_BufferHandle->wavefunc, audiobuf1, size1, bytes_per_sample, volume, generator);
				WriteWave<int16_t, 1>(m_BufferHandle->wavefunc, audiobuf2, size2, bytes_per_sample, volume, generator);
			}

			m_BufferHandle->cursor += (size1 + size2);
		}
		sound_buffer->Unlock(audiobuf1, size1, audiobuf2, size2);
	}

	void Win32Mixer::UpdateFile()
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
		uint8_t* filebuf = m_BufferHandle->file_payload.data.get();
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
					break;
				default:
					UpdateFile();
					break;
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
							LOG_INFO("Primary buffer created\n");
						}
					}

					IDirectSoundBuffer* secondary_buffer;
					if (direct_sound->CreateSoundBuffer(&buffer_desc2, &secondary_buffer, nullptr) >= 0) {
						LOG_INFO("Secondary buffer created\n");
						Win32BufferHandle* buffer_handle = new Win32BufferHandle;
						buffer_handle->buffer = secondary_buffer;
						buffer_handle->sample_rate = sample_rate;
						buffer_handle->channels = channels;
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

