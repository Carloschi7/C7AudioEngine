#pragma once 
#include <string>

#define PATH(path) std::string(C7AUDIOENGINE_ROOT_DIR) + '/' + std::string(path)
#define CPATH(path) (std::string(C7AUDIOENGINE_ROOT_DIR) + '/' + std::string(path)).c_str()
