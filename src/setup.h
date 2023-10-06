#pragma once 
#include <string>
#ifdef _MSC_VER
	#include <Windows.h>
#endif

#define PATH(path) std::string(C7AUDIOENGINE_ROOT_DIR) + '/' + std::string(path)
#define CPATH(path) (std::string(C7AUDIOENGINE_ROOT_DIR) + '/' + std::string(path)).c_str()
#ifdef _MSC_VER
#define LOG_ERROR(msg) OutputDebugStringA(msg); 
#define LOG_INFO(msg) OutputDebugStringA(msg); 
#else
#define LOG_ERROR(...)
#endif

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