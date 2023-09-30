cmake_minimum_required(VERSION 3.12)

project(C7AudioEngine)
set(C7_AUDIO_ENGINE_SOURCE 	
				src/win32_mixer.cpp
				src/win32_mixer.h
				src/linux_mixer.cpp
				src/linux_mixer.h
				src/mixer.cpp
				src/mixer.h)

if(MSVC)
    add_executable(${PROJECT_NAME} WIN32 ${C7_AUDIO_ENGINE_SOURCE})
    target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE)
endif()

add_definitions(-DROOT_DIR="${CMAKE_SOURCE_DIR}")

