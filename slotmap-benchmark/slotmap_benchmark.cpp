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

   ContainerType m_slotmap;
};


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

   inline ValueType& Get(KeyType key)
   {
      return m_stdMap[key];
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

#define MY_BENCHMARK(name_, traits_, traitsName_) \
   BENCHMARK_TEMPLATE(name_, traits_)->Name(#name_ "/" #traitsName_)ARGS;


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
   using KeyType = typename TContainer::KeyType;

   const int64_t count = state.range(0);
   std::vector<KeyType> keys;
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
         KeyType key = container.Insert(i);
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
MY_BENCHMARK(BM_InsertErase, SlotMapContainer<int>, SlotMap);
MY_BENCHMARK(BM_InsertErase, StdUnorderedMapContainer<int>, UnorderedMap);
MY_BENCHMARK(BM_InsertErase, VectorWithFreelist<int>, Vector);


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_InsertAccess(benchmark::State& state)
{
   BEFORE_BENCHMARK()

   using KeyType = typename TContainer::KeyType;
   using ValueType = typename TContainer::KeyType;

   const int64_t count = state.range(0);

   for (auto _ : state)
   {
      g_memCounterEnabled = true;

      TContainer container;
      uint64_t checksum = 0;

      for (size_t i = 0; i < count; ++i)
      {
         const ValueType value = i;
         const KeyType key = container.Insert(value);
         checksum += ++container.Get(key);
      }

      g_memCounterEnabled = false;
   }

   AFTER_BENCHMARK()
}
MY_BENCHMARK(BM_InsertAccess, SlotMapContainer<uint64_t>, SlotMap);
MY_BENCHMARK(BM_InsertAccess, StdUnorderedMapContainer<uint64_t>, UnorderedMap);
MY_BENCHMARK(BM_InsertAccess, VectorWithFreelist<uint64_t>, Vector);


//////////////////////////////////////////////////////////////////////////
template<typename TContainer>
void BM_Iteration(benchmark::State& state)
{
   BEFORE_BENCHMARK()


   const int64_t count = state.range(0);

   for (auto _ : state)
   {
      state.PauseTiming();

      TContainer container;
      for (size_t i = 0; i < count; ++i)
      {
         container.Insert(i);
      }

      state.ResumeTiming();

      g_memCounterEnabled = true;

      uint64_t checksum = 0;
      for (auto iter = container.Begin(); container.FindNext(iter); container.Increment(iter))
      {
         checksum += container.Get(iter);
      }

      g_memCounterEnabled = false;
   }

   AFTER_BENCHMARK()
}
MY_BENCHMARK(BM_Iteration, SlotMapContainer<uint64_t>, SlotMap);
MY_BENCHMARK(BM_Iteration, StdUnorderedMapContainer<uint64_t>, UnorderedMap);
MY_BENCHMARK(BM_Iteration, VectorWithFreelist<uint64_t>, Vector);


BENCHMARK_MAIN();

