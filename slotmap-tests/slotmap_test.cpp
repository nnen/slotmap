// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.

#include "test_common.h"

#include <slotmap/slotmap.h>

#include <gtest/gtest.h>

#include <queue>
#include <sstream>
#include <type_traits>
#include <unordered_set>


using namespace slotmap;


size_t TestValueType::s_ctorCount = 0;
size_t TestValueType::s_dtorCount = 0;


template<typename T>
struct SlotMapNameTraits
{
};


template<typename T, typename TKey>
struct SlotMapNameTraits<SlotMap<T, TKey, ChunkedSlotMapStorage<T, TKey>>>
{
   static void Get(std::ostream& out)
   {
      out << "SlotMap/";
      TypeNameTraits<TKey>::Get(out);
   }

   static void GetStorageInfo(std::ostream& out)
   {
      using Storage = ChunkedSlotMapStorage<T, TKey>;

      out << "Chunked:" << std::endl;
      out << "  Value size: " << sizeof(T) << std::endl;
      out << "  MaxChunkSlots: " << Storage::MaxChunkSlots << std::endl;
      out << "  ChunkSlots: " << Storage::ChunkSlots << std::endl;
      out << "  GenerationBitSize: " << Storage::GenerationBitSize << std::endl;
      out << "  SlotIndexBitSize: " << Storage::SlotIndexBitSize << std::endl;
      out << "  ChunkIndexBitSize: " << Storage::ChunkIndexBitSize;
   }
};


template<typename T, size_t TCapacity, typename TKey>
struct SlotMapNameTraits<SlotMap<T, TKey, FixedSlotMapStorage<T, TKey, TCapacity>>>
{
   static void Get(std::ostream& out)
   {
      out << "FixedSlotMap/";
      TypeNameTraits<TKey>::Get(out);
      out << "/" << TCapacity;
   }

   static void GetStorageInfo(std::ostream& out)
   {
      out << "Fixed";
   }
};


//////////////////////////////////////////////////////////////////////////
template<typename TSlotMap, size_t TMaxSize>
struct SlotMapTestTraits
{
   using MapType = TSlotMap;
   using ValueType = typename MapType::ValueType;
   using KeyType = typename MapType::KeyType;

   static constexpr size_t MaxSize = TMaxSize;

   static std::string GetName(int i)
   {
      std::stringstream ss;
      ss << i << "/";
      SlotMapNameTraits<TSlotMap>::Get(ss);
      ss << "/" << MaxSize;
      return ss.str();
   }
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
   using Pairs = std::unordered_map<KeyType, ValueType>;
   using Keys = std::unordered_set<KeyType>;

   static constexpr size_t MaxSize = Traits::MaxSize;

   virtual void SetUp() override
   {
      TestValueType::ResetCounters();
      m_valueCounter = 0;
   }

   ::testing::AssertionResult CheckMaxCapacity()
   {
      std::cerr << "Max capacity: " << MapType::MaxCapacity() << std::endl;

      SlotMapNameTraits<MapType>::GetStorageInfo(std::cerr);

      std::cerr << std::endl;

      if (MapType::MaxCapacity() < MaxSize)
      {
         return ::testing::AssertionFailure() << "Max capacity of the slotmap " << MapType::MaxCapacity() << 
            " is lower than the max size of the test " << MaxSize << "!";
      }
      return ::testing::AssertionSuccess();
   }
   
   ::testing::AssertionResult SetUpTestData(size_t count)
   {
      return SetUpTestData(m_map1, m_items, count);
   }

   ::testing::AssertionResult SetUpTestData(MapType& map, Pairs& values, size_t count)
   {
      for (size_t i = 0; i < count; ++i)
      {
         ++m_valueCounter;
         ValueType value = static_cast<ValueType>(m_valueCounter);
         const KeyType key = map.Emplace(value);
         if (values.count(key) != 0)
         {
            return ::testing::AssertionFailure() << "Key " << key << " already exists";
         }
         values[key] = value;
      }
      return ::testing::AssertionSuccess();
   }

   ::testing::AssertionResult SetUpTestData(MapType& map, Pairs& values, size_t count, float fillRate)
   {
      Keys keys;
      for (size_t i = 0; i < count; ++i)
      {
         ValueType value = static_cast<ValueType>(m_valueCounter++);
         const KeyType key = map.Emplace(value);
         if (values.count(key) != 0)
         {
            return ::testing::AssertionFailure() << "Key " << key << " already exists";
         }
         values[key] = value;
         keys.push_back(keys);
      }
      for (auto key : keys)
      {
         if (randf() < fillRate)
         {
            map.Erase(key);
            values.erase(key);
         }
      }
      return ::testing::AssertionSuccess();
   }

   ::testing::AssertionResult SetUpTestDataA(MapType& map, Pairs& values)
   {
      const size_t count = MaxSize >> 1;
      std::vector<KeyType> keys;

      for (size_t i = 0; i < count; ++i)
      {
         ValueType value = static_cast<ValueType>(m_valueCounter++);
         KeyType key{};
         ASSERT_R(Emplace(value, key, map, values));
         keys.push_back(key);
      }

      // First slot empty
      ASSERT_R(EraseValid(keys[0], map, values));

      // One slot empty
      ASSERT_R(EraseValid(keys[7], map, values));

      // 16 consecutive slots empty
      for (size_t i = 16; (i < 32) && (i < count); ++i)
      {
         ASSERT_R(EraseValid(keys[i], map, values));
      }

      // 64 consecutive slots empty aligned to 64
      for (size_t i = 64; (i < 128) && (i < count); ++i)
      {
         ASSERT_R(EraseValid(keys[i], map, values));
      }

      // 64 consecutive slots empty not aligned to 64
      for (size_t i = 160; (i < 224) && (i < count); ++i)
      {
         ASSERT_R(EraseValid(keys[i], map, values));
      }

      return ::testing::AssertionSuccess();
   }
   
   ::testing::AssertionResult CheckIteration()
   {
      return CheckIteration(m_map1);
   }

   ::testing::AssertionResult CheckIteration(const MapType& map)
   {
      Keys keys;
      
      for (KeyType iter = 0; map.FindNextKey(iter); iter = map.IncrementKey(iter))
      {
         const ValueType* ptr = map.GetPtr(iter);
         if (ptr == nullptr)
         {
            return ::testing::AssertionFailure() << "Iterating over a key " << iter << ", which is not present in the map.";
         }

         if (keys.count(iter) != 0)
         {
            return ::testing::AssertionFailure() << "Key " << iter << " already iterated";
         }
         keys.insert(iter);

         if (keys.size() > map.Size())
         {
            return ::testing::AssertionFailure() << "Iterated over more keys (" << keys.size() << ") than expected (" << map.Size() << ").";
         }
      }

      if (map.Size() != keys.size())
      {
         return ::testing::AssertionFailure() << "Iterated over less keys (" << keys.size() << ") than expected (" << map.Size() << ").";
      }

      return ::testing::AssertionSuccess();
   }

   ::testing::AssertionResult CheckIteration(const MapType& map, const Pairs& values)
   {
      Keys visitedKeys;

      for (KeyType key = 0; map.FindNextKey(key); key = map.IncrementKey(key))
      {
         const ValueType* ptr = map.GetPtr(key);
         if (ptr == nullptr)
         {
            return ::testing::AssertionFailure() << "Key " << key << " not found";
         }
         
         if (visitedKeys.count(key) != 0)
         {
            return ::testing::AssertionFailure() << "Key " << key << " already visited";
         }
         visitedKeys.insert(key);
         
         auto it = values.find(key);
         if (it == values.end())
         {
            return ::testing::AssertionFailure() << "Key " << key << " not found in expected values";
         }

         if (*ptr != it->second)
         {
            return ::testing::AssertionFailure() <<
               "Value " << *ptr << " does not match expected value " << it->second;
         }
      }

      if (values.size() != visitedKeys.size())
      {
         return ::testing::AssertionFailure() <<
            "Expected to iterate over " << values.size() << " keys, but iterated over " << visitedKeys.size();
      }

      return ::testing::AssertionSuccess();
   }
   
   ::testing::AssertionResult CheckIteration_Iterator()
   {
      return CheckIteration_Iterator(m_map1, m_items);
   }
   
   ::testing::AssertionResult CheckIteration_Iterator(const MapType& map, const Pairs& values)
   {
      Keys visitedKeys;
      size_t iteration = 0;

      for (auto it = map.Begin(); it != map.End(); ++it, ++iteration)
      {
         const ValueType* ptr = it.GetPtr();
         if (ptr == nullptr)
         {
            return ::testing::AssertionFailure() << "Key " << it.GetKey() << " not found (iteration " << iteration << ")";
         }
         
         if (visitedKeys.count(it.GetKey()) != 0)
         {
            return ::testing::AssertionFailure() << "Key " << it.GetKey() << " already visited (iteration " << iteration << ")";
         }
         visitedKeys.insert(it.GetKey());

         auto it2 = values.find(it.GetKey());
         if (it2 == values.end())
         {
            return ::testing::AssertionFailure() << "Key " << it.GetKey() << " not found in expected values (iteration " << iteration << ")";
         }

         if (*ptr != it2->second)
         {
            return ::testing::AssertionFailure() <<
               "Value " << *ptr << " does not match expected value " << it2->second << " (iteration " << iteration << ")";
         }
      }
      
      if (values.size() != visitedKeys.size())
      {
         return ::testing::AssertionFailure() <<
            "Expected to iterate over " << values.size() << " keys, but iterated over " << visitedKeys.size();
      }
      
      return ::testing::AssertionSuccess();
   }

   ::testing::AssertionResult CheckValues()
   {
      return CheckValues(m_map1, m_items);
   }
   
   ::testing::AssertionResult CheckValues(const MapType& map, const Pairs& values)
   {
      if (map.Size() != values.size())
      {
         return ::testing::AssertionFailure() << 
            "Map size " << map.Size() << " does not match values size " << values.size();
      }

      for (const auto& pair : values)
      {
         const ValueType* ptr = map.GetPtr(pair.first);
         if (ptr == nullptr)
         {
            return ::testing::AssertionFailure() << "Key " << pair.first << " not found";
         }
         if (!ptr->IsValid())
         {
            return ::testing::AssertionFailure() << "Value " << pair.second << " is invalid";
         }
         if (*ptr != pair.second)
         {
            return ::testing::AssertionFailure() << 
               "Value " << *ptr << " does not match expected value " << pair.second;
         }
      }

      return CheckIteration(map, values);
   }

   KeyType Emplace(const ValueType& value)
   {
      const KeyType key = m_map1.Emplace(value);
      m_items[key] = value;
      return key;
   }
   
   ::testing::AssertionResult Emplace(const ValueType& value, KeyType& outKey)
   {
      return Emplace(value, outKey, m_map1, m_items);
   }
   
   ::testing::AssertionResult Emplace(const ValueType& value, KeyType& outKey, MapType& map, Pairs& items)
   {
      outKey = map.Emplace(value);
      if (items.count(outKey) != 0)
      {
         return ::testing::AssertionFailure() << "Key " << outKey << " already exists";
      }
      items[outKey] = value;
      return ::testing::AssertionSuccess();
   }

   ::testing::AssertionResult EraseValid(KeyType key)
   {
      return EraseValid(key, m_map1, m_items);
   }

   ::testing::AssertionResult EraseValid(KeyType key, MapType& map, Pairs& items)
   {
      if (!map.Erase(key))
      {
         return ::testing::AssertionFailure() << "Failed to erase key " << key;
      }

      if (map.GetPtr(key) != nullptr)
      {
         return ::testing::AssertionFailure() << "Erased key " << key << " still exists";
      }
      
      if (map.Erase(key))
      {
         return ::testing::AssertionFailure() << "Erased key " << key << " twice";
      }
      
      auto it = items.find(key);
      if (it != items.end())
      {
         items.erase(it);
      }

      return ::testing::AssertionSuccess();
   }

   size_t  m_valueCounter;
   MapType m_map1;
   MapType m_map2;
   Pairs   m_items;
};


using SlotMapTestTypes = ::testing::Types<
   SlotMapTestTraits<FixedSlotMap<TestValueType, 64>, 64>,
   SlotMapTestTraits<FixedSlotMap<TestValueType, 255, uint16_t>, 255>,
   SlotMapTestTraits<FixedSlotMap<TestValueType, 1024>, 1024>,
   SlotMapTestTraits<FixedSlotMap<TestValueType, 1024, uint64_t>, 1024>,
   SlotMapTestTraits<SlotMap<TestValueType, uint16_t>, SlotMap<TestValueType, uint16_t>::MaxCapacity()>,
   SlotMapTestTraits<SlotMap<TestValueType>, 10000>,
   SlotMapTestTraits<SlotMap<TestValueType>, 1000000>,
   SlotMapTestTraits<SlotMap<TestValueType>, SlotMap<TestValueType>::MaxCapacity()>,
   SlotMapTestTraits<SlotMap<TestValueType, uint64_t>, 1000000>
>;
TYPED_TEST_SUITE(SlotMapTest, SlotMapTestTypes, TemplateTestNameGenerator);


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, CheckMaxCapacity)
{
   ASSERT_SUCCESS(this->CheckMaxCapacity());
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Create)
{
   using MapType = typename TestFixture::MapType;
   MapType map;
   ASSERT_EQ(0, map.Size());
   ASSERT_EQ(map.GetPtr(0), nullptr);
   ASSERT_EQ(map.GetPtr(1), nullptr);
   ASSERT_EQ(map.GetPtr(2), nullptr);
   ASSERT_EQ(map.GetPtr(MapType::InvalidKey), nullptr);

   ASSERT_TRUE(TestValueType::CheckLiveInstances(0));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, InvalidKey)
{
   using MapType = typename TestFixture::MapType;
   MapType map;
   ASSERT_EQ(0, map.Size());
   ASSERT_EQ(map.GetPtr(MapType::InvalidKey), nullptr);

   map.Emplace(123);
   
   ASSERT_EQ(map.GetPtr(MapType::InvalidKey), nullptr);
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, CopyCtor)
{
   using Traits = typename TestFixture::Traits;

   const size_t count = Traits::MaxSize >> 1;

   ASSERT_SUCCESS(TestFixture::SetUpTestData(this->m_map1, this->m_items, count));
   ASSERT_TRUE(TestValueType::CheckLiveInstances(count * 2));

   {
      typename TestFixture::MapType map(this->m_map1);
      ASSERT_TRUE(TestFixture::CheckValues(map, this->m_items));
      ASSERT_TRUE(TestFixture::CheckValues(this->m_map1, this->m_items));
      ASSERT_TRUE(TestValueType::CheckLiveInstances(count * 3));
   }

   ASSERT_TRUE(TestValueType::CheckLiveInstances(count * 2));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, MoveCtor_Empty)
{
   {
      typename TestFixture::MapType map(std::move(this->m_map1));
      ASSERT_TRUE(TestFixture::CheckValues(map, this->m_items));
      ASSERT_TRUE(TestFixture::CheckValues(this->m_map1, this->m_items));
      ASSERT_TRUE(TestValueType::CheckLiveInstances(0));
   }
   
   ASSERT_TRUE(TestValueType::CheckLiveInstances(0));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, MoveCtor)
{
   ASSERT_TRUE(TestFixture::SetUpTestData(this->m_map1, this->m_items, 16));
   ASSERT_TRUE(TestFixture::CheckValues(this->m_map1, this->m_items));
   ASSERT_TRUE(TestValueType::CheckLiveInstances(32));

   {
      typename TestFixture::MapType map(std::move(this->m_map1));
      ASSERT_TRUE(TestFixture::CheckValues(map, this->m_items));
      ASSERT_TRUE(TestValueType::CheckLiveInstances(32));
   }
   
   this->m_items.clear();
   ASSERT_TRUE(TestFixture::CheckValues(this->m_map1, this->m_items));
   
   ASSERT_TRUE(TestValueType::CheckLiveInstances(0));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, MoveAssignment)
{
   ASSERT_TRUE(TestFixture::SetUpTestData(this->m_map1, this->m_items, 16));
   ASSERT_TRUE(TestFixture::CheckValues(this->m_map1, this->m_items));
   ASSERT_TRUE(TestValueType::CheckLiveInstances(32));
   
   this->m_map2 = std::move(this->m_map1);
   
   ASSERT_TRUE(TestValueType::CheckLiveInstances(32));
   ASSERT_TRUE(TestFixture::CheckValues(this->m_map2, this->m_items));
   ASSERT_TRUE(TestFixture::CheckValues(this->m_map1, {}));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Emplace)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;

   {
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

      ASSERT_TRUE(TestValueType::CheckLiveInstances(3));
   }
   
   ASSERT_TRUE(TestValueType::CheckLiveInstances(0));
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
   keys.reserve(Traits::MaxSize);
   newKeys.reserve(Traits::MaxSize);

   // Fill the map
   for (size_t i = 0; i < Traits::MaxSize; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      keys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxSize, map.Size());

   // Check all keys are valid
   for (size_t i = 0; i < Traits::MaxSize; ++i)
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
   for (size_t i = 0; i < Traits::MaxSize; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      newKeys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxSize, map.Size());
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
   keys.reserve(Traits::MaxSize);

   // Fill the map
   for (size_t i = 0; i < Traits::MaxSize; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      ASSERT_TRUE(key != MapType::InvalidKey);
      keys.insert(key);
   }

   ASSERT_EQ(Traits::MaxSize, map.Size());
   
   const KeyType key = map.Emplace(123);
   ASSERT_TRUE(keys.count(key) == 0);
   ASSERT_TRUE(Traits::MaxSize <= map.Size());

   ASSERT_TRUE(TestFixture::CheckIteration(map));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, InsertAndErase)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   auto& map = TestFixture::m_map1;
   std::queue<KeyType> keyQueue;

   // Fill the map
   while (map.Size() < Traits::MaxSize)
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
TYPED_TEST(SlotMapTest, GetKeyByIndex)
{
   ASSERT_TRUE(TestFixture::SetUpTestDataA(this->m_map1, this->m_items));

   for (auto& pair : this->m_items)
   {
      const auto index = this->m_map1.GetIndexByKey(pair.first);
      ASSERT_LT(index, this->m_map1.Capacity());
      
      const auto key = this->m_map1.GetKeyByIndex(index);
      ASSERT_EQ(pair.first, key);
   }
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Swap)
{
   using Traits = typename TestFixture::Traits;

   const size_t count = Traits::MaxSize >> 1;

   ASSERT_TRUE(TestFixture::SetUpTestData(this->m_map1, this->m_items, count));
   ASSERT_TRUE(TestValueType::CheckLiveInstances(count * 2));

   this->m_map2.Swap(this->m_map1);

   ASSERT_TRUE(TestValueType::CheckLiveInstances(count * 2));
   ASSERT_TRUE(TestFixture::CheckValues(this->m_map2, this->m_items));
   ASSERT_TRUE(TestFixture::CheckValues(this->m_map1, {}));
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
   keys.reserve(Traits::MaxSize);

   for (size_t i = 0; i < Traits::MaxSize; ++i)
   {
      const KeyType key = map.Emplace(static_cast<int>(i));
      keys.push_back(key);
   }

   ASSERT_EQ(Traits::MaxSize, map.Size());
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

   size_t count = 0;
   map.ForEach([&](KeyType key, const ValueType& value) {
      ++count;
   });
   ASSERT_EQ(0, count);

   map.Emplace(123);

   count = 0;
   map.ForEach([&](KeyType key, const ValueType& value) {
      ++count;
   });
   ASSERT_EQ(1, count);
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Iteration_Empty)
{
   ASSERT_EQ(0, TestFixture::m_map1.Size());
   ASSERT_TRUE(TestFixture::CheckIteration());
}
 

//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Iteration_Filled)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   ASSERT_TRUE(TestFixture::SetUpTestData(TestFixture::MaxSize));
   ASSERT_TRUE(TestFixture::CheckIteration());
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Iteration_TestData)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   ASSERT_TRUE(TestFixture::SetUpTestDataA(this->m_map1, TestFixture::m_items));
   ASSERT_TRUE(TestFixture::CheckIteration());
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Iteration_Iterator)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   ASSERT_TRUE(TestFixture::SetUpTestData(TestFixture::MaxSize));
   ASSERT_TRUE(TestFixture::CheckIteration_Iterator());
}


