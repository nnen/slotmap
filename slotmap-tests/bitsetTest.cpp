// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.

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


using BitsetTestTypes = ::testing::Types<
   FixedBitset<64>,
   FixedBitset<1024>>;
TYPED_TEST_SUITE(BitsetTest, BitsetTestTypes);


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

