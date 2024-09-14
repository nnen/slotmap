// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.


#define MEMORY_PROFILER

#include <benchmark/benchmark.h>

#include <slotmap/slotmap.h>

#include <cstdlib>
#include <unordered_map>

#include "benchmark_common.h"
#include "plf_colony.h"


struct  
{
   void Clear()
   {
      m_allocCount = 0;
      m_freeCount = 0;
      m_allocBytes = 0;
      m_maxAllocSize = 0;
   }
   
   bool m_enabled = false;
   size_t m_allocCount = 0;
   size_t m_freeCount = 0;
   size_t m_allocBytes = 0;
   size_t m_maxAllocSize = 0;
} g_memCounters;


void* operator new(size_t size)
{
   if (g_memCounters.m_enabled)
   {
      ++g_memCounters.m_allocCount;
      g_memCounters.m_allocBytes += size;
      if (size > g_memCounters.m_maxAllocSize)
      {
         g_memCounters.m_maxAllocSize = size;
      }
   }

   return malloc(size);
}

void* operator new[](size_t size)
{
   if (g_memCounters.m_enabled)
   {
      ++g_memCounters.m_allocCount;
      g_memCounters.m_allocBytes += size;
      if (size > g_memCounters.m_maxAllocSize)
      {
         g_memCounters.m_maxAllocSize = size;
      }
   }
   
   return malloc(size);
}

void operator delete(void* ptr) noexcept
{
   ++g_memCounters.m_freeCount;

   free(ptr);
}


template<size_t Size = 64>
struct BenchmarkValue
{
   uint64_t m_value = 0;
   uint8_t m_padding[Size - sizeof(uint64_t)];

   inline BenchmarkValue() {}
   inline BenchmarkValue(uint64_t value)
      : m_value(value)
   {
   }
   
   inline operator uint64_t() const { return m_value; }
};


template<size_t Size>
inline bool operator==(const BenchmarkValue<Size>& a, const BenchmarkValue<Size>& b)
{
   return a.m_value == b.m_value;
}


template<size_t Size>
inline bool operator==(const BenchmarkValue<Size>& a, uint64_t b)
{
   return a.m_value == b;
}


template<typename T, 
         typename TBitsetTraits = slotmap::FixedBitSetTraits<>,
         typename TStorage = slotmap::ChunkedSlotMapStorage<T, uint32_t, slotmap::DefaultMaxChunkSize, std::allocator<T>, TBitsetTraits>>
class SlotMapContainer
{
public:
   using Storage = TStorage;
   using ContainerType = slotmap::SlotMap<T, uint32_t, Storage>;
   using ValueType = T;
   using KeyType = typename ContainerType::KeyType;
   using Iterator = KeyType;
   
   inline KeyType Insert(T value)
   {
      return m_slotmap.Emplace(value);
   }

   inline bool Erase(KeyType key)
   {
      return m_slotmap.Erase(key);
   }

   inline ValueType& Get(KeyType key)
   {
      return *m_slotmap.GetPtr(key);
   }

   inline void Reserve(size_t count)
   {
      m_slotmap.Reserve(count);
   }

   inline void Clear()
   {
      m_slotmap.Clear();
   }

   inline Iterator Begin()
   {
      return 0;
   }

   inline bool FindNext(Iterator& iter)
   {
      return m_slotmap.FindNextKey(iter);
   }

   inline void Increment(Iterator& iter)
   {
      iter = m_slotmap.IncrementKey(iter);
   }

   template<typename TFunc>
   inline void ForEach(TFunc func)
   {
      m_slotmap.ForEach(func);
   }

   ContainerType m_slotmap;
};


template<typename T, size_t TCapacity>
using FixedSlotMapContainer = SlotMapContainer<T, slotmap::FixedBitSetTraits<>, slotmap::FixedSlotMapStorage<T, uint32_t, TCapacity>>;
using FixedSlotMapContainer1000000 = FixedSlotMapContainer<uint64_t, 1000000>;


template<typename T>
class StdUnorderedMapContainer
{
public:
   using ValueType = T;
   using KeyType = int;
   using ContainerType = std::unordered_map<KeyType, T>;
   using Iterator = typename ContainerType::iterator;

   inline KeyType Insert(T value)
   {
      const int key = m_counter++;
      m_stdMap[key] = value;
      return key;
   }

   inline bool Erase(KeyType key)
   {
      auto it = m_stdMap.find(key);
      if (it == m_stdMap.end())
      {
         return false;
      }
      m_stdMap.erase(it);
      return true;
   }

   inline bool Erase(Iterator iterator)
   {
      m_stdMap.erase(iterator);
      return true;
   }

   inline ValueType& Get(KeyType key)
   {
      return m_stdMap[key];
   }

   inline void Reserve(size_t count)
   {
      m_stdMap.reserve(count);
   }

   inline void Clear()
   {
      m_stdMap.clear();
   }

   inline auto Begin()
   {
      return m_stdMap.begin();
   }

   inline bool FindNext(Iterator& iter)
   {
      return iter != m_stdMap.end();
   }

   inline void Increment(Iterator& iter)
   {
      ++iter;
   }

   inline ValueType& Get(Iterator& iter)
   {
      return iter->second;
   }

   int m_counter = 0;
   ContainerType m_stdMap;
};


template<typename T>
class VectorWithFreelist
{
public:
   using ValueType = T;
   using KeyType = size_t;
   using Iterator = size_t;

   inline KeyType Insert(T value)
   {
      if (m_freeList.empty())
      {
         const size_t key = m_values.size();
         m_values.push_back({ true, value });
         return key;
      }
      
      const size_t key = m_freeList.back();
      m_freeList.pop_back();
      Slot& slot = m_values[key];
      slot.m_isAlive = true;
      slot.m_value = value;
      return key;
   }
   
   inline bool Erase(KeyType key)
   {
      if ((key >= m_values.size()) || (!m_values[key].m_isAlive))
      {
         return false;
      }
      
      m_values[key].m_isAlive = false;
      m_freeList.push_back(key);
      
      return true;
   }
   
   inline ValueType& Get(KeyType key)
   {
      return m_values[key].m_value;
   }
   
   inline void Reserve(size_t cap)
   {
      if (m_values.size() >= cap)
      {
         return;
      }

      const size_t originalCap = m_values.size();

      m_values.resize(cap);
      
      for (size_t i = m_values.size(); i > originalCap; --i)
      {
         m_freeList.push_back(i - 1);
      }
   }
   
   inline void Clear()
   {
      m_values.clear();
      m_freeList.clear();
   }
   
   inline size_t Begin()    
   {
      return 0;
   }
   
   inline bool FindNext(size_t& iter)
   {
      while (iter < m_values.size())
      {
         if (m_values[iter].m_isAlive)
         {
            return true;
         }
         ++iter;
      }
      return false;
   }
   
   inline void Increment(size_t& iter)
   {
      ++iter;
   }
   
   struct Slot
   {
      bool m_isAlive = false;
      T m_value;
   };
   std::vector<Slot> m_values;
   std::vector<size_t> m_freeList;
};


template<typename T>
class ColonyContainer
{
public:
   using ContainerType = plf::colony<T>;
   using ValueType = T;
   using KeyType = typename ContainerType::iterator;

   inline KeyType Insert(T value)
   {
      return m_colony.insert(value);
   }

   inline bool Erase(KeyType key)
   {
      m_colony.erase(key);
      return true;
   }

   inline ValueType& Get(KeyType key)
   {
      return *key;
   }
   
   inline void Reserve(size_t count)
   {
      m_colony.reserve(count);
   }

   inline void Clear() 
   {
      m_colony.clear();
   }

   inline auto Begin()
   {
      return m_colony.begin();
   }

   inline bool FindNext(KeyType& iter)
   {
      return iter != m_colony.end();
   }

   inline void Increment(KeyType& iter)
   {
      ++iter;
   }
   
   ContainerType m_colony;
};


class CustomMemoryManager : public benchmark::MemoryManager
{
public:
   void Start() BENCHMARK_OVERRIDE
   {
      g_memCounters.Clear();
   }

   void Stop(Result& result) BENCHMARK_OVERRIDE
   {
      result.num_allocs = g_memCounters.m_allocCount;
      result.total_allocated_bytes = g_memCounters.m_allocBytes;
   }
};


#define ARGS ->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)

#define MY_BENCHMARK(name_, traits_, traitsName_) \
   BENCHMARK_TEMPLATE(name_, traits_)->Name(#name_ "/" #traitsName_)ARGS


#define BEFORE_BENCHMARK() \
   g_memCounters.Clear();

#define AFTER_BENCHMARK() \
   state.counters["Alloc count"] = benchmark::Counter(static_cast<double>(g_memCounters.m_allocCount), benchmark::Counter::kAvgIterations); \
   state.counters["Free count"] = benchmark::Counter(static_cast<double>(g_memCounters.m_freeCount), benchmark::Counter::kAvgIterations); \
   state.counters["Alloc bytes"] = benchmark::Counter(static_cast<double>(g_memCounters.m_allocBytes), benchmark::Counter::kAvgIterations); \
   state.counters["Max alloc size"] = static_cast<double>(g_memCounters.m_maxAllocSize);

#define ENABLE_MEM_COUNTERS() g_memCounters.m_enabled = true;
#define DISABLE_MEM_COUNTERS() g_memCounters.m_enabled = false;


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void SetupRandom(TContainer& container, size_t capacity, float fillRatio)
{
   container.Reserve(capacity);

   if (fillRatio >= 1.0f)
   {
      for (size_t i = 0; i < capacity; ++i)
      {
         container.Insert(i);
      }
   }
   else if (fillRatio > 0.0f)
   {
      std::srand(239480239);

      for (size_t i = 0; i < capacity; ++i)
      {
         container.Insert(i);
      }

      for (auto iter = container.Begin(); container.FindNext(iter); )
      {
         auto next = iter;
         container.Increment(next);

         if (randf() >= fillRatio)
         {
            container.Erase(iter);
         }

         iter = next;
      }
   }
}

template<typename TContainer>
void SetupPartiallyFilled(TContainer& container, size_t capacity, float fillRatio)
{
   const size_t count = capacity * fillRatio;

   container.Reserve(capacity);

   for (size_t i = 0; i < count; ++i)
   {
      container.Insert(i);
   }
}


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_InsertErase(benchmark::State& state)
{
   using ValueType = typename TContainer::ValueType;
   using KeyType = typename TContainer::KeyType;

   const int64_t count = state.range(0);
   std::vector<KeyType> keys;
   keys.resize(count);

   BEFORE_BENCHMARK()

   for (auto _ : state)
   {
      ENABLE_MEM_COUNTERS();

      TContainer container;

      for (size_t i = 0; i < static_cast<size_t>(count); ++i)
      {
         container.Insert(static_cast<ValueType>(i));
      }

      container.Clear();
      
      for (size_t i = 0; i < static_cast<size_t>(count); ++i)
      {
         KeyType key = container.Insert(static_cast<ValueType>(i));
         DISABLE_MEM_COUNTERS();
         keys[i] = key;
         ENABLE_MEM_COUNTERS();
      }
      
      for (size_t i = 0; i < static_cast<size_t>(count); ++i)
      {
         container.Erase(keys[i]);
      }
      
      DISABLE_MEM_COUNTERS();
   }

   AFTER_BENCHMARK()
}
MY_BENCHMARK(BM_InsertErase, SlotMapContainer<int>, SlotMap);
MY_BENCHMARK(BM_InsertErase, StdUnorderedMapContainer<int>, UnorderedMap);
MY_BENCHMARK(BM_InsertErase, VectorWithFreelist<int>, Vector);
MY_BENCHMARK(BM_InsertErase, ColonyContainer<int>, Colony);


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_InsertAccess(benchmark::State& state)
{
   const int64_t count = state.range(0);

   BEFORE_BENCHMARK()

   using KeyType = typename TContainer::KeyType;
   using ValueType = typename TContainer::ValueType;

   for (auto _ : state)
   {
      ENABLE_MEM_COUNTERS();

      TContainer container;
      volatile uint64_t checksum = 0;

      for (size_t i = 0; i < static_cast<size_t>(count); ++i)
      {
         const ValueType value = static_cast<ValueType>(i);
         const KeyType key = container.Insert(value);
         checksum += ++container.Get(key);
      }

      DISABLE_MEM_COUNTERS();
   }

   AFTER_BENCHMARK()
}
MY_BENCHMARK(BM_InsertAccess, SlotMapContainer<uint64_t>, SlotMap);
MY_BENCHMARK(BM_InsertAccess, StdUnorderedMapContainer<uint64_t>, UnorderedMap);
MY_BENCHMARK(BM_InsertAccess, VectorWithFreelist<uint64_t>, Vector);
MY_BENCHMARK(BM_InsertAccess, ColonyContainer<uint64_t>, Colony);


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_Clear(benchmark::State& state, const size_t count, float fillRatio)
{
   auto container = std::make_unique<TContainer>();

   for (auto _ : state)
   {
      state.PauseTiming();
      
      SetupPartiallyFilled(*container, count, fillRatio);

      state.ResumeTiming();

      container->Clear();
   }
}

template<typename TContainer>
void BM_Clear(benchmark::State& state)
{
   const float fillRatio = static_cast<float>(state.range(0)) / 100.0f;
   const size_t count = static_cast<size_t>(state.range(1));
   
   BM_Clear<TContainer>(state, count, fillRatio);
}

#undef ARGS
#define ARGS ->ArgsProduct({{0, 25, 50, 75, 100}, {1000000}})->Unit(benchmark::kMicrosecond)->Iterations(10)
MY_BENCHMARK(BM_Clear, SlotMapContainer<uint64_t>, SlotMap);
MY_BENCHMARK(BM_Clear, FixedSlotMapContainer1000000, FixedSlotMap);
MY_BENCHMARK(BM_Clear, StdUnorderedMapContainer<uint64_t>, UnorderedMap);
MY_BENCHMARK(BM_Clear, VectorWithFreelist<uint64_t>, Vector);
MY_BENCHMARK(BM_Clear, ColonyContainer<uint64_t>, Colony);


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_Iteration_Only(benchmark::State& state, TContainer& container)
{
   for (auto _ : state)
   {
      volatile uint64_t checksum = 0;
      for (auto iter = container.Begin(); container.FindNext(iter); container.Increment(iter))
      {
         checksum += container.Get(iter);
      }
   }
}

template<typename TContainer>
void BM_Iteration_ForEachOnly(benchmark::State& state, TContainer& container)
{
   for (auto _ : state)
   {
      volatile uint64_t checksum = 0;
      container.ForEach([&checksum](typename TContainer::KeyType id, const typename TContainer::ValueType& value)
      {
         checksum += value;
      });
   }
}

template<typename TContainer>
void BM_Iteration(benchmark::State& state, const size_t count, const float fillRatio)
{
   TContainer container;

   SetupRandom(container, count, fillRatio);
   
   BM_Iteration_Only(state, container);
}


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_Iteration_ForEach(benchmark::State& state, const size_t count, const float fillRatio)
{
   TContainer container;

   SetupRandom(container, count, fillRatio);
      
   BM_Iteration_ForEachOnly(state, container);
}


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_Iteration(benchmark::State& state)
{
   const float fillRatio = static_cast<float>(state.range(0)) / 100.0f;
   const size_t count = static_cast<size_t>(state.range(1));

   BM_Iteration<TContainer>(state, count, fillRatio);
}

template<typename TContainer>
void BM_Iteration_ForEach(benchmark::State& state)
{
   const float fillRatio = static_cast<float>(state.range(0)) / 100.0f;
   const size_t count = static_cast<size_t>(state.range(1));

   BM_Iteration_ForEach<TContainer>(state, count, fillRatio);
}

template<typename TContainer>
void BM_Iteration_PartiallyFilled(benchmark::State& state)
{
   const float fillRatio = static_cast<float>(state.range(0)) / 100.0f;
   const size_t capacity = static_cast<size_t>(state.range(1));
   const size_t count = capacity * fillRatio;

   auto container = std::make_unique<TContainer>();

   SetupPartiallyFilled(*container, capacity, fillRatio);

   BM_Iteration_Only(state, *container);
}

template<typename TContainer>
void BM_Iteration_PartiallyFilledForEach(benchmark::State& state)
{
   const float fillRatio = static_cast<float>(state.range(0)) / 100.0f;
   const size_t capacity = static_cast<size_t>(state.range(1));
   const size_t count = capacity * fillRatio;

   auto container = std::make_unique<TContainer>();
   
   SetupPartiallyFilled(*container, capacity, fillRatio);
   
   BM_Iteration_ForEachOnly(state, *container);
}

#undef ARGS
#define ARGS ->ArgsProduct({{0, 25, 50, 75, 100}, {1000000}})->Unit(benchmark::kMicrosecond)
MY_BENCHMARK(BM_Iteration, SlotMapContainer<BenchmarkValue<>>, SlotMap);
MY_BENCHMARK(BM_Iteration_ForEach, SlotMapContainer<BenchmarkValue<>>, SlotMap);
using SlotMapContainerStdBitset = SlotMapContainer<BenchmarkValue<>, slotmap::StdBitSetTraits>;
MY_BENCHMARK(BM_Iteration, SlotMapContainerStdBitset, SlotMapStdBitset);
MY_BENCHMARK(BM_Iteration, StdUnorderedMapContainer<BenchmarkValue<>>, UnorderedMap);
MY_BENCHMARK(BM_Iteration, VectorWithFreelist<BenchmarkValue<>>, Vector);
MY_BENCHMARK(BM_Iteration, ColonyContainer<BenchmarkValue<>>, Colony);

MY_BENCHMARK(BM_Iteration_PartiallyFilled, SlotMapContainer<BenchmarkValue<>>, SlotMap);
MY_BENCHMARK(BM_Iteration_PartiallyFilledForEach, SlotMapContainer<BenchmarkValue<>>, SlotMap);
MY_BENCHMARK(BM_Iteration_PartiallyFilled, SlotMapContainerStdBitset, SlotMapStdBitset);
MY_BENCHMARK(BM_Iteration_PartiallyFilled, FixedSlotMapContainer1000000, FixedSlotMap);
MY_BENCHMARK(BM_Iteration_PartiallyFilledForEach, FixedSlotMapContainer1000000, FixedSlotMap);
MY_BENCHMARK(BM_Iteration_PartiallyFilled, StdUnorderedMapContainer<BenchmarkValue<>>, UnorderedMap);
MY_BENCHMARK(BM_Iteration_PartiallyFilled, VectorWithFreelist<BenchmarkValue<>>, Vector);
MY_BENCHMARK(BM_Iteration_PartiallyFilled, ColonyContainer<BenchmarkValue<>>, Colony);


BENCHMARK_MAIN();

