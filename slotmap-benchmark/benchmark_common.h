// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.


#pragma once
#include <cstdint>
#include <cstdlib>


inline float randf()
{
   return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

