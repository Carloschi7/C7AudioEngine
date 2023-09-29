cmake_minimum_required(VERSION 3.12)

project(C7AudioEngine)
set(C7_AUDIO_ENGINE_SOURCE 	src/win32_loader.cpp
				src/win32_loader.h
				src/linux_loader.cpp
				src/linux_loader.h
				src/loader.cpp
				src/loader.h)

if(MSVC)
    add_executable(${PROJECT_NAME} WIN32 ${C7_AUDIO_ENGINE_SOURCE})
    target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE)
endif()

add_definitions(-DROOT_DIR="${CMAKE_SOURCE_DIR}")

