cmake_minimum_required(VERSION 3.12)

project(C7AudioEngine)
set(C7_AUDIO_ENGINE_SOURCE
				src/setup.h
				src/wav.h
				src/wav.cpp
				src/mp3.h
				src/mp3.cpp
				src/win32_mixer.cpp
				src/win32_mixer.h
				src/linux_mixer.cpp
				src/linux_mixer.h
				src/mixer.cpp
				src/mixer.h)

#Third party program folders

if(MSVC)
    add_library(${PROJECT_NAME} STATIC ${C7_AUDIO_ENGINE_SOURCE})
    target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE)
else()
	#TODO linux
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/externals)
add_definitions(-DC7AUDIOENGINE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

