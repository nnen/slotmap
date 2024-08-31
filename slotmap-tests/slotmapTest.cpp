#include "testsCommon.h"

#include <slotmap/slotmap.h>

#include <gtest/gtest.h>

#include <unordered_set>
#include <queue>


using namespace slotmap;


//////////////////////////////////////////////////////////////////////////
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


inline std::ostream& operator<<(std::ostream& os, const TestValueType& value)
{
   return os << value.m_value;
}

inline bool operator==(const TestValueType& value, int32_t i) { return value.m_value == i; }
inline bool operator==(int32_t i, const TestValueType& value) { return value.m_value == i; }
inline bool operator!=(const TestValueType& value, int32_t i) { return !(value == i); }
inline bool operator!=(int32_t i, const TestValueType& value) { return !(value == i); }


size_t TestValueType::s_ctorCount = 0;
size_t TestValueType::s_dtorCount = 0;


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
   using Pairs = std::unordered_map<KeyType, ValueType>;
   using Keys = std::unordered_set<KeyType>;

   virtual void SetUp() override
   {
      TestValueType::ResetCounters();
      m_valueCounter = 0;
   }

   ::testing::AssertionResult SetUpTestData(size_t count)
   {
      return SetUpTestData(m_map1, m_items, count);
   }

   ::testing::AssertionResult SetUpTestData(MapType& map, Pairs& values, size_t count)
   {
      for (size_t i = 0; i < count; ++i)
      {
         ValueType value = static_cast<ValueType>(m_valueCounter++);
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
      for (size_t i = 0; i < count; ++i)
      {
         ValueType value = static_cast<ValueType>(m_valueCounter++);
         const KeyType key = map.Emplace(value);
         if (values.count(key) != 0)
         {
            return ::testing::AssertionFailure() << "Key " << key << " already exists";
         }
         values[key] = value;
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
            "Counter " << visitedKeys.size() << " does not match values size " << values.size();
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
      outKey = m_map1.Emplace(value);
      if (m_items.count(outKey) != 0)
      {
         return ::testing::AssertionFailure() << "Key " << outKey << " already exists";
      }
      m_items[outKey] = value;
      return ::testing::AssertionSuccess();
   }

   ::testing::AssertionResult EraseValid(KeyType key)
   {
      if (!m_map1.Erase(key))
      {
         return ::testing::AssertionFailure() << "Failed to erase key " << key;
      }

      if (m_map1.GetPtr(key) != nullptr)
      {
         return ::testing::AssertionFailure() << "Erased key " << key << " still exists";
      }
      
      if (m_map1.Erase(key))
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
   SlotMapTestTraits<SlotMap<TestValueType>, 10000>,
   SlotMapTestTraits<SlotMap<TestValueType>, 1000000>,
   SlotMapTestTraits<SlotMap<TestValueType, uint64_t>, 1000000>>;
TYPED_TEST_SUITE(SlotMapTest, SlotMapTestTypes);


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Create)
{
   typename TestFixture::MapType map;
   ASSERT_EQ(0, map.Size());
   ASSERT_EQ(map.GetPtr(0), nullptr);
   ASSERT_EQ(map.GetPtr(1), nullptr);
   ASSERT_EQ(map.GetPtr(2), nullptr);

   ASSERT_TRUE(TestValueType::CheckLiveInstances(0));
}


//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, CopyCtor)
{
   using Traits = typename TestFixture::Traits;

   const size_t count = Traits::MaxCap >> 1;

   ASSERT_TRUE(TestFixture::SetUpTestData(this->m_map1, this->m_items, count));
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

   auto& map = TestFixture::m_map1;
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
TYPED_TEST(SlotMapTest, Swap)
{
   using Traits = typename TestFixture::Traits;

   const size_t count = Traits::MaxCap >> 1;

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
   ASSERT_EQ(0, TestFixture::m_map1.Size());
   ASSERT_TRUE(TestFixture::CheckIteration());
}
 

//////////////////////////////////////////////////////////////////////////
TYPED_TEST(SlotMapTest, Iteration)
{
   using MapType = typename TestFixture::MapType;
   using KeyType = typename MapType::KeyType;
   using ValueType = typename MapType::ValueType;
   using Traits = typename TestFixture::Traits;

   ASSERT_TRUE(TestFixture::SetUpTestData(Traits::MaxCap));
   ASSERT_TRUE(TestFixture::CheckIteration());
}



