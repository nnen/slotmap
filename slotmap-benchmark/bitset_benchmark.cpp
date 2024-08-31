#include <benchmark/benchmark.h>

#include <bitset>
#include <cstdlib>

#include <slotmap/bitset.h>

#include "benchmark_common.h"


template<size_t BitsetSize>
struct StdBitsetTraits
{
   static constexpr size_t Size = BitsetSize;
   using BitsetType = std::bitset<BitsetSize>;

   static inline void Set(BitsetType& bitset, size_t index, bool value)
   {
      bitset.set(index, value);
   }

   static inline bool Get(BitsetType& bitset, size_t index)
   {
      return bitset.test(index);
   }

   static inline size_t FindNextBitSet(const BitsetType& bitset, size_t start)
   {
      return slotmap::StdBitSetTraits::FindNextBitSet(bitset, start);
   }
};


template<size_t BitsetSize>
struct FixedBitsetTraits
{
   static constexpr size_t Size = BitsetSize;
   using BitsetType = slotmap::FixedBitset<BitsetSize>;

   static inline void Set(BitsetType& bitset, size_t index, bool value)
   {
      bitset.Set(index, value);
   }

   static inline bool Get(BitsetType& bitset, size_t index)
   {
      return bitset.Get(index);
   }
   
   static inline size_t FindNextBitSet(const BitsetType& bitset, size_t start)
   {
      return bitset.FindNextBitSet(start);
   }
};


template<typename Traits>
void BM_Bitset_Set(benchmark::State& state)
{
   using BitsetType = typename Traits::BitsetType;

   BitsetType bitset;

   for (auto _ : state)
   {
      for (size_t i = 0; i < Traits::Size; ++i)
      {
         Traits::Set(bitset, i, true);
      }
   }
}


template<typename Traits>
void BM_Bitset_Iteration(benchmark::State& state, float fillRatio)
{
   using BitsetType = typename Traits::BitsetType;

   std::srand(239480239);

   BitsetType bitset;
   size_t count = 0;
   if (fillRatio > 0.0f)
   {
      for (size_t i = 0; i < Traits::Size; ++i)
      {
         if (randf() < fillRatio)
         {
            Traits::Set(bitset, i, true);
            ++count;
         }
      }
   }
   else if (fillRatio >= 1.0f)
   {
      for (size_t i = 0; i < Traits::Size; ++i)
      {
         Traits::Set(bitset, i, true);
      }
      count = Traits::Size;
   }
   
   for (auto _ : state)
   {
      volatile size_t checksum = 0;
      for (size_t i = Traits::FindNextBitSet(bitset, 0); i < Traits::Size; i = Traits::FindNextBitSet(bitset, i + 1))
      {
         ++checksum;
      }
   }
}


template<typename Traits>
void BM_Bitset_Iteration_ForEach(benchmark::State& state, float fillRatio)
{
   using BitsetType = typename Traits::BitsetType;

   std::srand(239480239);

   BitsetType bitset;
   size_t count = 0;
   if (fillRatio > 0.0f)
   {
      for (size_t i = 0; i < Traits::Size; ++i)
      {
         if (randf() < fillRatio)
         {
            Traits::Set(bitset, i, true);
            ++count;
         }
      }
   }
   else if (fillRatio >= 1.0f)
   {
      for (size_t i = 0; i < Traits::Size; ++i)
      {
         Traits::Set(bitset, i, true);
      }
      count = Traits::Size;
   }
   
   for (auto _ : state)
   {
      volatile size_t checksum = 0;
      bitset.ForEachSetBit([&checksum](size_t index)
      {
         ++checksum;
      });
   }
}


template<typename Traits>
void BM_Bitset_Iteration(benchmark::State& state)
{
   const float fillRatio = static_cast<float>(state.range(0)) / 100.0f;
   BM_Bitset_Iteration<Traits>(state, fillRatio);
}


template<typename Traits>
void BM_Bitset_Iteration_ForEach(benchmark::State& state)
{
   const float fillRatio = static_cast<float>(state.range(0)) / 100.0f;
   BM_Bitset_Iteration_ForEach<Traits>(state, fillRatio);
}


BENCHMARK_TEMPLATE(BM_Bitset_Set, StdBitsetTraits<1000000>);
BENCHMARK_TEMPLATE(BM_Bitset_Set, FixedBitsetTraits<1000000>);

BENCHMARK_TEMPLATE(BM_Bitset_Iteration, StdBitsetTraits<1000000>)
   ->DenseRange(0, 100, 10);
BENCHMARK_TEMPLATE(BM_Bitset_Iteration, FixedBitsetTraits<1000000>)
   ->DenseRange(0, 100, 10);
BENCHMARK_TEMPLATE(BM_Bitset_Iteration_ForEach, FixedBitsetTraits<1000000>)
   ->DenseRange(0, 100, 10);

