// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.


#pragma once
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <string>


//////////////////////////////////////////////////////////////////////////
inline float randf()
{
   return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}


//////////////////////////////////////////////////////////////////////////
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


//////////////////////////////////////////////////////////////////////////
template<typename T>
struct TypeNameTraits
{
   static void Get(std::ostream& out)
   {
      if constexpr (std::is_integral_v<T>)
      {
         if constexpr (std::is_unsigned_v<T>)
         {
            out << "u";
         }
         else
         {
            out << "i";
         }
         out << sizeof(T) * CHAR_BIT;
      }
      else
      {
         out << "?";
      }
   }
};


#define TYPE_NAME(type_) \
   template<> \
   struct TypeNameTraits<type_> \
   { \
      static void Get(std::ostream& out) \
      { \
         out << #type_; \
      } \
   };


template<typename T>
void GetTypeName(std::stringstream& ss)
{
   TypeNameTraits<T>::Get(ss);
}


template<typename T>
std::string GetTypeName()
{
   std::stringstream ss;
   GetTypeName(ss);
   return ss.str();
}


//////////////////////////////////////////////////////////////////////////
struct TemplateTestNameGenerator {
   template <typename T>
   static std::string GetName(int i) 
   {
      return T::GetName(i);
   }
};


//////////////////////////////////////////////////////////////////////////
/**
 * Helper class to check the proper lifetime handling of values.
 */
struct TestValueType
{
   static constexpr uint32_t Sentinel_DefaultCtor = 0xCAFEBABEu;
   static constexpr uint32_t Sentinel_Ctor = 0xBEEFBABEu;
   static constexpr uint32_t Sentinel_CopyCtor = 0xBEEFBEEFu;
   static constexpr uint32_t Sentinel_MoveCtor = 0xBABEB00B;
   static constexpr uint32_t Sentinel_Dtor = 0xDEADBABEu;
   static constexpr uint32_t Sentinel_Moved = 0xDEADFA11u;
   
   TestValueType() { ++s_ctorCount; }
   TestValueType(int32_t value) 
      : m_value(value)
      , m_sentinel(Sentinel_Ctor) 
   { 
      ++s_ctorCount; 
   }
   TestValueType(size_t value)
      : m_value(static_cast<int32_t>(value))
      , m_sentinel(Sentinel_Ctor)
   {
      ++s_ctorCount;
   }
   TestValueType(const TestValueType& other) 
      : m_value(other.m_value)
      , m_sentinel(Sentinel_CopyCtor) 
   { 
      ++s_ctorCount;
   }
   TestValueType(TestValueType&& other) 
      : m_value(other.m_value)
      , m_sentinel(Sentinel_MoveCtor) 
   { 
      other.m_sentinel = Sentinel_Moved; 
      ++s_ctorCount; 
   }
   ~TestValueType() 
   {
      assert(IsValid());
      m_sentinel = Sentinel_Dtor;
      ++s_dtorCount;
   }
   
   inline TestValueType& operator=(const TestValueType& other) { m_value = other.m_value; return *this; }
   inline TestValueType& operator=(TestValueType&& other) { m_value = other.m_value; other.m_sentinel = Sentinel_Moved; return *this; }
   
   inline bool operator==(const TestValueType& other) const { return m_value == other.m_value; }
   inline bool operator!=(const TestValueType& other) const { return m_value != other.m_value; }
   
   inline bool IsValid() const 
   { 
      return (m_sentinel == Sentinel_Ctor) ||
         (m_sentinel == Sentinel_DefaultCtor) ||
         (m_sentinel == Sentinel_CopyCtor) ||
         (m_sentinel == Sentinel_MoveCtor) ||
         (m_sentinel == Sentinel_Moved);
   }
   inline bool IsDestroyed() const { return m_sentinel == Sentinel_Dtor; }

   static void ResetCounters() 
   { 
      s_ctorCount = 0; 
      s_dtorCount = 0; 
   }

   static ::testing::AssertionResult CheckLiveInstances(size_t expectedCount)
   {
      if (s_dtorCount > s_ctorCount)
      {
         return ::testing::AssertionFailure() << "Destructor count " << s_dtorCount << " exceeds constructor count " << s_ctorCount;
      }
      const size_t liveCount = s_ctorCount - s_dtorCount;
      if (liveCount != expectedCount)
      {
         return ::testing::AssertionFailure() << "Live instance count " << liveCount << " does not match expected count " << expectedCount;
      }
      return ::testing::AssertionSuccess();
   }

   int32_t m_value = 0;
   uint32_t m_sentinel = Sentinel_DefaultCtor;
   
   static size_t s_ctorCount;
   static size_t s_dtorCount;
};

TYPE_NAME(TestValueType);

inline std::ostream& operator<<(std::ostream& os, const TestValueType& value)
{
   return os << value.m_value;
}

inline bool operator==(const TestValueType& value, int32_t i) { return value.m_value == i; }
inline bool operator==(int32_t i, const TestValueType& value) { return value.m_value == i; }
inline bool operator!=(const TestValueType& value, int32_t i) { return !(value == i); }
inline bool operator!=(int32_t i, const TestValueType& value) { return !(value == i); }


//////////////////////////////////////////////////////////////////////////
template<size_t N>
struct TestValueTpl : public TestValueType
{
   using TestValueType::TestValueType;

   uint8_t m_padding[N];
};

