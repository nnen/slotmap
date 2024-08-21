#include <slotmap/slotmap.h>

#include <gtest/gtest.h>

#include <unordered_set>


using namespace slotmap;


//////////////////////////////////////////////////////////////////////////
template<typename TSlotMap, size_t TMaxCap>
struct SlotMapTestTraits
{
   using MapType = TSlotMap;
   using ValueType = typename MapType::ValueType;
   using KeyType = typename MapType::KeyType;

   static constexpr size_t MaxCap = TMaxCap;
};


//////////////////////////////////////////////////////////////////////////
template<typename TTraits>
class SlotMapTest : public testing::Test
{
public:
   using MapType = typename TTraits::MapType;
   using ValueType = typename MapType::ValueType;
   using KeyType = typename MapType::KeyType;
   using Traits = TTraits;
};

using SlotMapTestTypes = ::testing::Types<
   SlotMapTestTraits<FixedSlotMap<int, 64>, 64>,
   SlotMapTestTraits<FixedSlotMap<int, 1024>, 1024>,
   SlotMapTestTraits<SlotMap<int>, 10000>>;
TYPED_TEST_SUITE(SlotMapTest, SlotMapTestTypes);


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Placeholder)
{
   typename TestFixture::MapType map;
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Fixed)
{
   typename TestFixture::MapType map;
   ASSERT_EQ(0, map.Size());
   ASSERT_EQ(map.GetPtr(0), nullptr);
   ASSERT_EQ(map.GetPtr(1), nullptr);
   ASSERT_EQ(map.GetPtr(2), nullptr);
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Emplace)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;

   MapType map;
   ASSERT_EQ(0, map.Size());

   KeyType k1 = map.Emplace(123);
   ASSERT_EQ(1, map.Size());
   ASSERT_NE(MapType::InvalidKey, k1);

   KeyType k2 = map.Emplace(234);
   ASSERT_EQ(2, map.Size());
   ASSERT_NE(MapType::InvalidKey, k2);

   KeyType k3 = map.Emplace(345);
   ASSERT_EQ(3, map.Size());
   ASSERT_NE(MapType::InvalidKey, k3);

   const ValueType* value = map.GetPtr(k1);
   ASSERT_NE(nullptr, value);
   ASSERT_EQ(123, *value);

   value = map.GetPtr(k2);
   ASSERT_NE(nullptr, value);
   ASSERT_EQ(234, *value);

   value = map.GetPtr(k3);
   ASSERT_NE(nullptr, value);
   ASSERT_EQ(345, *value);
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Erase)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;

   MapType map;
   ASSERT_EQ(0, map.Size());

   std::vector<KeyType> keys;

   for (size_t i = 0; i < 16; ++i)
   {
      keys.push_back(map.Emplace(static_cast<int>(i)));
      const ValueType* ptr = map.GetPtr(keys.back());
      ASSERT_NE(nullptr, ptr);
      ASSERT_EQ(static_cast<int>(i), *ptr);
   }

   ASSERT_EQ(16, map.Size());

   for (KeyType key : keys)
   {
      ASSERT_TRUE(map.Erase(key));
      ASSERT_FALSE(map.Erase(key));
      ASSERT_FALSE(map.Erase(key));
   }

   ASSERT_EQ(0, map.Size());
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Fill)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   MapType map;
   std::vector<KeyType> keys;
   std::vector<KeyType> newKeys;
   keys.reserve(Traits::MaxCap);
   newKeys.reserve(Traits::MaxCap);

   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      keys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxCap, map.Size());

   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = keys[i];
      const ValueType* ptr = map.GetPtr(key);
      if (static_cast<int>(i) != *ptr)
      {
         ASSERT_EQ(static_cast<int>(i), *ptr);
      }
   }
   
   for (KeyType key : keys)
   {
      ASSERT_TRUE(map.Erase(key));
      ASSERT_FALSE(map.Erase(key));
   }

   ASSERT_EQ(0, map.Size());

   for (KeyType key : keys)
   {
      const ValueType* ptr = map.GetPtr(key);
      ASSERT_EQ(nullptr, ptr);
      ASSERT_FALSE(map.Erase(key));
   }

   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      newKeys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxCap, map.Size());
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Clear)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   MapType map;
   std::vector<KeyType> keys;
   keys.reserve(Traits::MaxCap);

   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      keys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxCap, map.Size());
   map.Clear();
   ASSERT_EQ(0, map.Size());

   for (KeyType key : keys)
   {
      const ValueType* ptr = map.GetPtr(key);
      ASSERT_EQ(nullptr, ptr);
   }

   for (KeyType key : keys)
   {
      ASSERT_FALSE(map.Erase(key));
   }
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Iteration)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   MapType map;

   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      map.Emplace(static_cast<int>(i));
   }

   ASSERT_EQ(Traits::MaxCap, map.Size());

   size_t counter = 0;
   std::unordered_set<KeyType> keys;

   for (KeyType iter = 0; map.FindNextKey(iter); iter = map.IncrementKey(iter))
   {
      ++counter;
      ASSERT_TRUE(keys.count(iter) == 0);
      keys.insert(iter);
      ASSERT_TRUE(counter <= map.Size());
   }

   ASSERT_EQ(Traits::MaxCap, counter);
}



