#include <slotmap/slotmap.h>

#include <gtest/gtest.h>

#include <unordered_set>
#include <queue>


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

   ::testing::AssertionResult CheckIteration()
   {
      return CheckIteration(m_map);
   }

   ::testing::AssertionResult CheckIteration(const MapType& map)
   {
      size_t counter = 0;
      std::unordered_set<KeyType> keys;
      
      for (KeyType iter = 0; map.FindNextKey(iter); iter = map.IncrementKey(iter))
      {
         ++counter;
         if (keys.count(iter) != 0)
         {
            return ::testing::AssertionFailure() << "Key " << iter << " already iterated";
         }
         keys.insert(iter);
         if (counter > map.Size())
         {
            return ::testing::AssertionFailure() << "Counter " << counter << " exceeds map size " << map.Size();
         }
      }

      if (map.Size() != counter)
      {
         return ::testing::AssertionFailure() << "Counter " << counter << " does not match map size " << map.Size();
      }

      return ::testing::AssertionSuccess();
   }

   KeyType Emplace(const ValueType& value)
   {
      const KeyType key = m_map.Emplace(value);
      m_items[key] = value;
      return key;
   }
   
   ::testing::AssertionResult Emplace(const ValueType& value, KeyType& outKey)
   {
      outKey = m_map.Emplace(value);
      if (m_items.count(outKey) != 0)
      {
         return ::testing::AssertionFailure() << "Key " << outKey << " already exists";
      }
      m_items[outKey] = value;
      return ::testing::AssertionSuccess();
   }

   ::testing::AssertionResult EraseValid(KeyType key)
   {
      if (!m_map.Erase(key))
      {
         return ::testing::AssertionFailure() << "Failed to erase key " << key;
      }

      if (m_map.GetPtr(key) != nullptr)
      {
         return ::testing::AssertionFailure() << "Erased key " << key << " still exists";
      }
      
      if (m_map.Erase(key))
      {
         return ::testing::AssertionFailure() << "Erased key " << key << " twice";
      }
      
      auto it = m_items.find(key);
      if (it != m_items.end())
      {
         m_items.erase(it);
      }

      return ::testing::AssertionSuccess();
   }

   MapType m_map;
   std::unordered_map<KeyType, ValueType> m_items;
};

using SlotMapTestTypes = ::testing::Types<
   SlotMapTestTraits<FixedSlotMap<int, 64>, 64>,
   SlotMapTestTraits<FixedSlotMap<int, 255, uint16_t>, 255>,
   SlotMapTestTraits<FixedSlotMap<int, 1024>, 1024>,
   SlotMapTestTraits<FixedSlotMap<int, 1024, uint64_t>, 1024>,
   SlotMapTestTraits<SlotMap<int>, 10000>,
   SlotMapTestTraits<SlotMap<int>, 1000000>,
   SlotMapTestTraits<SlotMap<int, uint64_t>, 1000000>>;
TYPED_TEST_SUITE(SlotMapTest, SlotMapTestTypes);


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Create)
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

   // Fill the map
   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      keys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxCap, map.Size());

   // Check all keys are valid
   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = keys[i];
      const ValueType* ptr = map.GetPtr(key);
      if (static_cast<int>(i) != *ptr)
      {
         ASSERT_EQ(static_cast<int>(i), *ptr);
      }
   }
   
   // Erase all keys
   for (KeyType key : keys)
   {
      ASSERT_TRUE(map.Erase(key));
      ASSERT_FALSE(map.Erase(key));
   }

   ASSERT_EQ(0, map.Size());

   // Check all keys are invalid
   for (KeyType key : keys)
   {
      const ValueType* ptr = map.GetPtr(key);
      ASSERT_EQ(nullptr, ptr);
      ASSERT_FALSE(map.Erase(key));
   }

   // Fill the map again
   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      newKeys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxCap, map.Size());
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Overfill)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   MapType map;
   std::unordered_set<KeyType> keys;
   keys.reserve(Traits::MaxCap);

   // Fill the map
   for (size_t i = 0; i < Traits::MaxCap; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      keys.insert(key);
   }

   ASSERT_EQ(Traits::MaxCap, map.Size());
   
   const KeyType key = map.Emplace(123);
   ASSERT_TRUE(keys.count(key) == 0);
   ASSERT_TRUE(Traits::MaxCap <= map.Size());

   ASSERT_TRUE(TestFixture::CheckIteration(map));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, InsertAndErase)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   auto& map = TestFixture::m_map;
   std::queue<KeyType> keyQueue;

   // Fill the map
   while (map.Size() < Traits::MaxCap)
   {
      KeyType key;

      ASSERT_TRUE(TestFixture::Emplace(static_cast<int>(map.Size()), key));
      keyQueue.push(key);
      ASSERT_EQ(keyQueue.size(), map.Size());

      key = keyQueue.front();
      ASSERT_TRUE(TestFixture::EraseValid(key));
      keyQueue.pop();
      ASSERT_EQ(keyQueue.size(), map.Size());

      ASSERT_TRUE(TestFixture::Emplace(static_cast<int>(map.Size()), key));
      keyQueue.push(key);
      ASSERT_EQ(keyQueue.size(), map.Size());
   }
   
   // Empty the map
   while (keyQueue.size() > 0)
   {
      KeyType key = keyQueue.front();
      ASSERT_TRUE(TestFixture::EraseValid(key));
      keyQueue.pop();
      ASSERT_EQ(keyQueue.size(), map.Size());

      ASSERT_TRUE(TestFixture::Emplace(static_cast<int>(map.Size()), key));
      keyQueue.push(key);
      ASSERT_EQ(keyQueue.size(), map.Size());

      key = keyQueue.front();
      ASSERT_TRUE(TestFixture::EraseValid(key));
      keyQueue.pop();
      ASSERT_EQ(keyQueue.size(), map.Size());
   }

   ASSERT_EQ(0, map.Size());
   ASSERT_TRUE(TestFixture::CheckIteration(map));
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
TYPED_TEST(SlotMapTest, Iteration_Empty)
{
   ASSERT_EQ(0, TestFixture::m_map.Size());
   ASSERT_TRUE(TestFixture::CheckIteration());
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

   ASSERT_TRUE(TestFixture::CheckIteration(map));
}



