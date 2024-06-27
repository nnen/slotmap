#define MEMORY_PROFILER

#include <benchmark/benchmark.h>
#include <cstdlib>

#include <slotmap/slotmap.h>

#include <unordered_map>


bool g_memCounterEnabled = false;
size_t g_memAllocCount = 0;
size_t g_memAllocBytes = 0;
size_t g_maxAllocSize = 0;


void* operator new(size_t size)
{
   if (g_memCounterEnabled)
   {
      ++g_memAllocCount;
      g_memAllocBytes += size;
      if (size > g_maxAllocSize)
      {
         g_maxAllocSize = size;
      }
   }
   return malloc(size);
}


template<typename T>
class SlotMapContainer
{
public:
   using ContainerType = slotmap::SlotMap<T>;
   using ValueType = T;
   using KeyType = typename ContainerType::KeyType;

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

   inline void Clear()
   {
      m_slotmap.Clear();
   }

   ContainerType m_slotmap;
};


template<typename T>
class StdUnorderedMapContainer
{
public:
   using ValueType = T;
   using KeyType = int;

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

   inline ValueType& Get(KeyType key)
   {
      return m_stdMap[key];
   }

   inline void Clear()
   {
      m_stdMap.clear();
   }

   int m_counter = 0;
   std::unordered_map<KeyType, T> m_stdMap;
};


template<typename T>
class VectorWithFreelist
{
public:
   using ValueType = T;
   using KeyType = size_t;

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

   inline void Clear()
   {
      m_values.clear();
      m_freeList.clear();
   }

   struct Slot
   {
      bool m_isAlive = false;
      T m_value;
   };
   std::vector<Slot> m_values;
   std::vector<size_t> m_freeList;
};


class CustomMemoryManager : public benchmark::MemoryManager
{
public:
   //size_t m_allocCount = 0;
   //size_t m_allocBytes = 0;

   void Start() BENCHMARK_OVERRIDE
   {
      g_memAllocCount = 0;
      g_memAllocBytes = 0;
   }

   void Stop(Result& result) BENCHMARK_OVERRIDE
   {
      result.num_allocs = g_memAllocCount;
      result.total_allocated_bytes = g_memAllocBytes;
   }
};


#define ARGS ->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)


#define BEFORE_BENCHMARK() \
   g_memAllocBytes = 0; \
   g_memAllocCount = 0; \
   g_maxAllocSize = 0;

#define AFTER_BENCHMARK() \
   state.counters["Alloc count"] = benchmark::Counter(g_memAllocCount, benchmark::Counter::kAvgIterations); \
   state.counters["Alloc bytes"] = benchmark::Counter(g_memAllocBytes, benchmark::Counter::kAvgIterations); \
   state.counters["Max alloc size"] = g_maxAllocSize;


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_InsertErase(benchmark::State& state)
{
   const int64_t count = state.range(0);
   std::vector<TContainer::KeyType> keys;
   keys.resize(count);

   BEFORE_BENCHMARK()

   for (auto _ : state)
   {
      g_memCounterEnabled = true;

      TContainer container;

      for (size_t i = 0; i < count; ++i)
      {
         container.Insert(i);
      }

      container.Clear();

      for (size_t i = 0; i < count; ++i)
      {
         TContainer::KeyType key = container.Insert(i);
         g_memCounterEnabled = false;
         keys[i] = key;
         g_memCounterEnabled = true;
      }

      for (size_t i = 0; i < count; ++i)
      {
         container.Erase(keys[i]);
      }
      
      state.PauseTiming();
      keys.clear();
      state.ResumeTiming();

      g_memCounterEnabled = false;
   }

   AFTER_BENCHMARK()
}
BENCHMARK(BM_InsertErase<SlotMapContainer<int>>)ARGS;
BENCHMARK(BM_InsertErase<StdUnorderedMapContainer<int>>)ARGS;
BENCHMARK(BM_InsertErase<VectorWithFreelist<int>>)ARGS;


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_InsertAccess(benchmark::State& state)
{
   BEFORE_BENCHMARK()

   const int64_t count = state.range(0);

   for (auto _ : state)
   {
      g_memCounterEnabled = true;

      TContainer container;
      uint64_t checksum = 0;

      for (size_t i = 0; i < count; ++i)
      {
         const TContainer::ValueType value = i;
         const TContainer::KeyType key = container.Insert(value);
         checksum += ++container.Get(key);
      }

      g_memCounterEnabled = false;
   }

   AFTER_BENCHMARK()
}
BENCHMARK(BM_InsertAccess<SlotMapContainer<uint64_t>>)ARGS;
BENCHMARK(BM_InsertAccess<StdUnorderedMapContainer<uint64_t>>)ARGS;
BENCHMARK(BM_InsertAccess<VectorWithFreelist<uint64_t>>)ARGS;


BENCHMARK_MAIN();
