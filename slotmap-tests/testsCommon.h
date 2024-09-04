// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.


#pragma once
#include <cstdint>
#include <cstdlib>


inline float randf()
{
   return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}


#define ASSERT_SUCCESS(expr_) \
   GTEST_AMBIGUOUS_ELSE_BLOCKER_ \
   if (const ::testing::AssertionResult gtest_ar_ = (expr_)) { \
   } else { \
      GTEST_FATAL_FAILURE_(#expr_) << gtest_ar_.message(); \
   }

#define ASSERT_R(expr_) \
   GTEST_AMBIGUOUS_ELSE_BLOCKER_ \
   if (const ::testing::AssertionResult gtest_ar = (expr_)) { \
   } else { \
      return gtest_ar; \
   }

