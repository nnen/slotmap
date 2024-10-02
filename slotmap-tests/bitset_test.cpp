// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.

#include "test_common.h"

#include <slotmap/bitset.h>

#include <gtest/gtest.h>


using namespace slotmap;


template<typename TBitset>
class BitsetTest : public ::testing::Test
{
public:
   using BitsetType = TBitset;
   using IndexList = std::vector<size_t>;

   ::testing::AssertionResult CheckBitset(const BitsetType& bitset, const IndexList& expected)
   {
      size_t next = 0;
      for (size_t i = 0; i < bitset.size(); ++i)
      {
         if ((next < expected.size()) && (expected[next] == i))
         {
            if (!bitset.test(i))
            {
               return ::testing::AssertionFailure() << "Expected bit " << i << " to be set but it is not";
            }
            ++next;
         }
         else
         {
            if (bitset.test(i))
            {
               return ::testing::AssertionFailure() << "Expected bit " << i << " to be unset but it is set";
            }
         }
      }

      next = 0;
      for (size_t i = bitset.FindNextBitSet(0); i < bitset.size(); i = bitset.FindNextBitSet(i + 1))
      {
         if (next >= expected.size())
         {
            return ::testing::AssertionFailure() << "Unexpected bit " << i << " is set";
         }
         if (expected[next] != i)
         {
            return ::testing::AssertionFailure() << "Expected bit " << expected[next] << " but got " << i;
         }
         ++next;
      }
      if (next < expected.size())
      {
         return ::testing::AssertionFailure() << "Expected " << expected.size() << " bits set but got " << next;
      }

      next = 0;
      bool forEachSuccess = true;
      bitset.ForEachSetBit([&](size_t index)
      {
         if (next >= expected.size())
         {
            forEachSuccess = false;
         }
         if (expected[next] != index)
         {
            forEachSuccess = false;
         }
         ++next;
      });
      if (!forEachSuccess)
      {
         return ::testing::AssertionFailure() << "ForEachSetBit iterated over wrong bits." << next;
      }
      if (next < expected.size())
      {
         return ::testing::AssertionFailure() << "Expected " << expected.size() << " bits set but got " << next;
      }

      return ::testing::AssertionSuccess();
   }
};


struct BitsetTestNameGenerator {
   template <typename T>
   static std::string GetName(int i) 
   {
      using BitsetType = T;

      std::stringstream ss;
      ss << BitsetType::StaticSize;
      return ss.str();
   }
};


using BitsetTestTypes = ::testing::Types<
   FixedBitset<64>,
   FixedBitset<1024>>;
TYPED_TEST_SUITE(BitsetTest, BitsetTestTypes, BitsetTestNameGenerator);


TYPED_TEST(BitsetTest, Empty)
{
   EXPECT_TRUE(TestFixture::CheckBitset({}, {}));
}


TYPED_TEST(BitsetTest, Bits)
{
   using Bitset = typename TestFixture::BitsetType;
   using IndexList = typename TestFixture::IndexList;

   Bitset bitset;
   IndexList indexes;

   bitset.set(0);
   indexes.push_back(0); 

   bitset.set(32);
   indexes.push_back(32); 

   bitset.set(63);
   indexes.push_back(63); 

   std::bitset<1024> stdBitset;
   stdBitset.set(0);
   stdBitset.set(32);
   stdBitset.set(63);

   EXPECT_TRUE(TestFixture::CheckBitset(bitset, indexes));
}


TYPED_TEST(BitsetTest, Fill)
{
   using Bitset = typename TestFixture::BitsetType;

   Bitset bitset;

   for (size_t i = 0; i < bitset.size(); ++i)
   {
      bitset.set(i);
   }
   
   for (size_t i = 0; i < bitset.size(); ++i)
   {
      EXPECT_TRUE(bitset.test(i));
   }

   bitset.reset();

   for (size_t i = 0; i < bitset.size(); ++i)
   {
      EXPECT_FALSE(bitset.test(i));
   }
}


TYPED_TEST(BitsetTest, ForEachSetBit_FromTo)
{
   using Bitset = typename TestFixture::BitsetType;

   Bitset bitset;

   size_t counter = 0;
   bitset.ForEachSetBit(0, 0, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(0, counter);

   bitset.set(32);

   counter = 0;
   bitset.ForEachSetBit(0, 0, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(0, counter);

   counter = 0;
   bitset.ForEachSetBit(0, 64, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(1, counter);

   counter = 0;
   bitset.ForEachSetBit(0, 32, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(0, counter);
   
   counter = 0;
   bitset.ForEachSetBit(32, 64, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(1, counter);

   counter = 0;
   bitset.ForEachSetBit(20, 40, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(1, counter);

   bitset.set(33);
   bitset.set(63);

   counter = 0;
   bitset.ForEachSetBit(20, 40, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(2, counter);

   counter = 0;
   bitset.ForEachSetBit(0, 64, [&](size_t index)
   {
      ++counter;
   });
   ASSERT_EQ(3, counter);
}

