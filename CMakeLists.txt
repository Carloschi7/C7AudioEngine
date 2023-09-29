cmake_minimum_required(VERSION 3.12)

project(C7AudioEngine)
set(C7_AUDIO_ENGINE_SOURCE src/loader.cpp)

if(MSVC)
    add_library(${PROJECT_NAME} STATIC ${C7_AUDIO_ENGINE_SOURCE})
endif()