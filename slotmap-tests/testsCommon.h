#pragma once
#include <cstdint>


inline float randf()
{
   return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

