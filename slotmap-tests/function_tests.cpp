// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.

#include "test_common.h"

#include <slotmap/slotmap.h>

#include <gtest/gtest.h>


using namespace slotmap;


//////////////////////////////////////////////////////////////////////////
template<typename TValue, typename TIndex, typename TGeneration, typename TBitsetTraits>
::testing::AssertionResult TestGetChunkMaxSlots()
{
	constexpr size_t SlotCount = ::slotmap::impl::GetChunkMaxSlots<
      ::slotmap::MinChunkSlots, ::slotmap::DefaultMaxChunkSize, ::slotmap::DefaultMaxChunkSize,
      TValue, TIndex, TGeneration, TBitsetTraits>();

   using ChunkType = ::slotmap::ChunkTpl<SlotCount, TValue, TIndex, TGeneration, TBitsetTraits>;
   using ChunkTypePlusOne = ::slotmap::ChunkTpl<SlotCount + 1, TValue, TIndex, TGeneration, TBitsetTraits>;

   static_assert((SlotCount <= ::slotmap::MinChunkSlots) || (sizeof(ChunkType) <= ::slotmap::DefaultMaxChunkSize));
   static_assert(sizeof(ChunkTypePlusOne) > ::slotmap::DefaultMaxChunkSize);
  
   return ::testing::AssertionSuccess();
}

TEST(GetChunkSizeTest, GetChunkSize)
{
#define TEST_(...) { auto result = TestGetChunkMaxSlots<__VA_ARGS__>(); EXPECT_TRUE(result); }
	TEST_(int,           ptrdiff_t, uint8_t,  FixedBitSetTraits<>)
	TEST_(int,           ptrdiff_t, uint16_t, FixedBitSetTraits<>)
	TEST_(uint64_t,      ptrdiff_t, uint8_t,  FixedBitSetTraits<>)
	TEST_(TestValueType, ptrdiff_t, uint8_t,  FixedBitSetTraits<>)
	TEST_(TestValueType, ptrdiff_t, uint16_t, FixedBitSetTraits<>)
	TEST_(TestValueTpl<::slotmap::DefaultMaxChunkSize>, ptrdiff_t, uint8_t,  FixedBitSetTraits<>)
	TEST_(uint64_t,      ptrdiff_t, uint8_t,  StdBitSetTraits)
	TEST_(TestValueType, ptrdiff_t, uint8_t,  StdBitSetTraits)
#undef TEST_
}


//////////////////////////////////////////////////////////////////////////
TEST(GetIndexBitSizeTest, GetIndexBitSize)
{
   EXPECT_EQ(impl::GetIndexBitSize(8), 3);
   EXPECT_EQ(impl::GetIndexBitSize(9), 4);
   EXPECT_EQ(impl::GetIndexBitSize(15), 4);
   EXPECT_EQ(impl::GetIndexBitSize(16), 4);
   EXPECT_EQ(impl::GetIndexBitSize(std::numeric_limits<uint16_t>::max()), std::numeric_limits<uint16_t>::digits);
   EXPECT_EQ(impl::GetIndexBitSize(std::numeric_limits<uint32_t>::max()), std::numeric_limits<uint32_t>::digits);
   EXPECT_EQ(impl::GetIndexBitSize(std::numeric_limits<uint64_t>::max()), std::numeric_limits<uint64_t>::digits);
   EXPECT_EQ(impl::GetIndexBitSize(std::numeric_limits<uintmax_t>::max()), std::numeric_limits<uintmax_t>::digits);
   EXPECT_EQ(impl::GetIndexBitSize(std::numeric_limits<unsigned int>::max()), std::numeric_limits<unsigned int>::digits);
}

