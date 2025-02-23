// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com) - All rights reserved.

#pragma once
#include <bitset>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <type_traits>

#ifdef _MSC_VER
#include <intrin.h>
#endif


namespace slotmap {


//////////////////////////////////////////////////////////////////////////
/**
 * Counts the number of trailing zeros in an unsigned integer.
 *
 * This is equivalent to 0-based index of the least significant bit that is set.
 */
template<typename T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
inline int CountTrailingZeros(T x)
{
#ifdef _MSC_VER
   static_assert(sizeof(T) <= sizeof(uint64_t), "Unsupported integer type.");
   unsigned long r;
   if constexpr(sizeof(T) <= sizeof(unsigned long))
      _BitScanForward(&r, x);
   else
      _BitScanForward64(&r, x);
   return static_cast<int>(r);
#else
   static_assert(sizeof(T) <= sizeof(unsigned long long), "Unsupported integer type.");
   if constexpr(sizeof(T) <= sizeof(unsigned int))
      return __builtin_ctz(x);
   else if constexpr(sizeof(T) <= sizeof(unsigned long))
      return __builtin_ctzl(x);
   else
      return __builtin_ctzll(x);
#endif
}


//////////////////////////////////////////////////////////////////////////
/**
 * This is a fixed bitset implementation very similar to `std::bitset`, but
 * with support for fast iteration over set bits using built-in functions.
 *
 * The main reason for a custom implementation is that standard `std::bitset`
 * doesn't support fast iteration over set bits using a built-in function like
 * `__builtin_ctzll`. This is a little bit unfortunate, because actual
 * implementations of `std::bitset` do implement this function (e.g.
 * `std::bitset` in libstdc++ implements `_M_do_find_next`), but it's not part
 * of the standard interface.
 */
template<size_t TSize, typename TWord = uintptr_t>
class FixedBitset
{
public:
   static_assert(std::is_unsigned_v<TWord>, "The word type must be an unsigned integer type.");

   using WordType = TWord;

   static constexpr size_t StaticSize = TSize;
   static constexpr size_t BitsPerWord = sizeof(TWord) * CHAR_BIT;
   static constexpr size_t NumWords = (StaticSize + BitsPerWord - 1) / BitsPerWord;
   static constexpr size_t BitIndexMask = BitsPerWord - 1;

   FixedBitset();
   FixedBitset(const FixedBitset& other);
   FixedBitset(FixedBitset&& other);

   FixedBitset& operator=(const FixedBitset& other);
   FixedBitset& operator=(FixedBitset&& other);

   inline bool operator[](size_t index) const { return Get(index); }

   inline WordType* Data() { return m_words; }
   inline const WordType* Data() const { return m_words; }

   constexpr bool Get(size_t index) const;
   void Set(size_t index);
   void Unset(size_t index);
   void Set(size_t index, bool value);
   void Flip(size_t index);
   void Flip();

   inline size_t FindNextBitSet(size_t start) const;

   template<typename TFunc>
   void ForEachSetBit(TFunc func) const;

   template<typename TFunc>
   void ForEachSetBit(size_t from, size_t to, TFunc func) const;
   
   void Clear();

   static constexpr size_t GetWordIndex(size_t index) { return index / BitsPerWord; }
   static constexpr size_t GetBitIndex(size_t index) { return index & BitIndexMask; }

   // STL-compatibility functions
   // STL Element access
   inline bool test(size_t index) const { return Get(index); }
   // STL Capacity
   inline size_t size() const { return StaticSize; }
   // STL Modifiers
   inline void set(size_t index) { Set(index); }
   inline void reset(size_t index) { Unset(index); }
   inline void reset() { Clear(); }
   inline void flip(size_t index) { Flip(index); }
   inline void flip() { Flip(); }

private:
   TWord m_words[NumWords];
};


//////////////////////////////////////////////////////////////////////////
template<typename TWord = uintptr_t>
struct FixedBitSetTraits
{
   template<size_t TSize>
   using BitsetType = FixedBitset<TSize, TWord>;
   
   template<size_t TSize>
   static inline size_t FindNextBitSet(const BitsetType<TSize>& bitset, size_t start)
   {
      return bitset.FindNextBitSet(start);
   }

   template<size_t TSize, typename TFunc>
   static inline void ForEachSetBit(const BitsetType<TSize>& bitset, TFunc func)
   {
      bitset.ForEachSetBit(func);
   }

   template<size_t TSize, typename TFunc>
   static inline void ForEachSetBit(size_t from, size_t to, const BitsetType<TSize>& bitset, TFunc func)
   {
      bitset.ForEachSetBit(from, to, func);
   }
};


struct StdBitSetTraits
{
   template<size_t TSize>
   using BitsetType = std::bitset<TSize>;
   
   template<size_t TSize>
   static inline size_t FindNextBitSet(const BitsetType<TSize>& bitset, size_t start)
   {
      for (size_t i = start; i < TSize; ++i)
      {
         if (bitset.test(i))
         {
            return i;
         }
      }
      return TSize;
   }

   template<size_t TSize, typename TFunc>
   static inline void ForEachSetBit(const BitsetType<TSize>& bitset, TFunc func)
   {
      for (size_t i = 0; i < TSize; ++i)
      {
         if (bitset.test(i))
         {
            func(i);
         }
      }
   }

   template<size_t TSize, typename TFunc>
   static inline void ForEachSetBit(size_t from, size_t to, const BitsetType<TSize>& bitset, TFunc func)
   {
      for (size_t i = from; i < to; ++i)
      {
         if (bitset.test(i))
         {
            func(i);
         }
      }
   }
};


//////////////////////////////////////////////////////////////////////////
template<size_t TSize, typename TWord>
FixedBitset<TSize, TWord>::FixedBitset()
{
   Clear();
}


template<size_t TSize, typename TWord>
FixedBitset<TSize, TWord>::FixedBitset(const FixedBitset& other)
{
   memcpy(m_words, other.m_words, sizeof(m_words));
}


template<size_t TSize, typename TWord>
FixedBitset<TSize, TWord>::FixedBitset(FixedBitset&& other)
{
   memcpy(m_words, other.m_words, sizeof(m_words));
}


template<size_t TSize, typename TWord>
FixedBitset<TSize, TWord>& FixedBitset<TSize, TWord>::operator=(FixedBitset&& other)
{
   memcpy(m_words, other.m_words, sizeof(m_words));
   return *this;
}


template<size_t TSize, typename TWord>
constexpr bool FixedBitset<TSize, TWord>::Get(size_t index) const
{
   const size_t wordIndex = index / BitsPerWord;
   assert(wordIndex < NumWords);
   return (m_words[wordIndex] & (static_cast<TWord>(1u) << (index & BitIndexMask))) != 0;
}


template<size_t TSize, typename TWord>
void FixedBitset<TSize, TWord>::Set(size_t index)
{
   const size_t wordIndex = index / BitsPerWord;
   assert(wordIndex < NumWords);
   m_words[wordIndex] |= (static_cast<TWord>(1u) << (index & BitIndexMask));
}


template<size_t TSize, typename TWord>
void FixedBitset<TSize, TWord>::Unset(size_t index)
{
   const size_t wordIndex = index / BitsPerWord;
   assert(wordIndex < NumWords);
   m_words[wordIndex] &= ~(static_cast<TWord>(1u) << (index & BitIndexMask));
}


template<size_t TSize, typename TWord>
void FixedBitset<TSize, TWord>::Set(size_t index, bool value)
{
   const size_t wordIndex = index / BitsPerWord;
   assert(wordIndex < NumWords);
   if (value)
   {
      m_words[wordIndex] |= (static_cast<TWord>(1u) << (index & BitIndexMask));
   }
   else
   {
      m_words[wordIndex] &= ~(static_cast<TWord>(1u) << (index & BitIndexMask));
   }
}


template<size_t TSize, typename TWord>
void FixedBitset<TSize, TWord>::Flip(size_t index)
{
   const size_t wordIndex = index / BitsPerWord;
   assert(wordIndex < NumWords);
   m_words[wordIndex] ^= (static_cast<TWord>(1u) << (index & BitIndexMask));
}


template<size_t TSize, typename TWord>
void FixedBitset<TSize, TWord>::Flip()
{
   for (size_t i = 0; i < NumWords; ++i)
   {
      m_words[i] = ~m_words[i];
   }
}


template<size_t TSize, typename TWord>
size_t FixedBitset<TSize, TWord>::FindNextBitSet(size_t start) const
{
   size_t wordIndex = GetWordIndex(start);

   const TWord word = m_words[wordIndex] >> GetBitIndex(start);

   if (word != static_cast<TWord>(0))
   {
      const size_t bitIndex = CountTrailingZeros(word);
      return start + bitIndex;
   }

   ++wordIndex;

   for (; wordIndex < NumWords; ++wordIndex)
   {
      const TWord word = m_words[wordIndex];
      if (word != static_cast<TWord>(0))
      {
         const size_t bitIndex = CountTrailingZeros(word);
         return wordIndex * BitsPerWord + bitIndex;
      }
   }
   
   return StaticSize;
}


template<size_t TSize, typename TWord>
template<typename TFunc>
void FixedBitset<TSize, TWord>::ForEachSetBit(TFunc func) const
{
   for (size_t wordIndex = 0; wordIndex < NumWords; ++wordIndex)
   {
      TWord word = m_words[wordIndex];
      while (word != static_cast<TWord>(0))
      {
         const size_t bitIndex = CountTrailingZeros(word);
         func(wordIndex * BitsPerWord + bitIndex);
         word &= ~(static_cast<TWord>(1u) << bitIndex);
      }
   }
}


template<size_t TSize, typename TWord>
template<typename TFunc>
void FixedBitset<TSize, TWord>::ForEachSetBit(size_t from, size_t to, TFunc func) const
{
   if (from >= to)
   {
      return;
   }
   
   const size_t fromWordIndex = from / BitsPerWord;
   const size_t fromBitIndex = from & BitIndexMask;
   const size_t toWordIndex = to / BitsPerWord;
   const size_t toBitIndex = to & BitIndexMask;

   if (fromWordIndex == toWordIndex)
   {
      TWord word = m_words[fromWordIndex] & ~((static_cast<TWord>(1u) << fromBitIndex) - 1);
      while (word != static_cast<TWord>(0))
      {
         const size_t bitIndex = CountTrailingZeros(word);
         if (bitIndex >= toBitIndex)
         {
            return;
         }
         func(fromWordIndex * BitsPerWord + bitIndex);
         word &= ~(static_cast<TWord>(1u) << bitIndex);
      }
   }
   else
   {
      TWord word = m_words[fromWordIndex] & ~((static_cast<TWord>(1u) << fromBitIndex) - 1);
      while (word != static_cast<TWord>(0))
      {
         const size_t bitIndex = CountTrailingZeros(word);
         func(fromWordIndex * BitsPerWord + bitIndex);
         word &= ~(static_cast<TWord>(1u) << bitIndex);
      }
      
      for (size_t wordIndex = fromWordIndex + 1; wordIndex < toWordIndex; ++wordIndex)
      {
         word = m_words[wordIndex];
         while (word != static_cast<TWord>(0))
         {
            const size_t bitIndex = CountTrailingZeros(word);
            func(wordIndex * BitsPerWord + bitIndex);
            word &= ~(static_cast<TWord>(1u) << bitIndex);
         }
      }
      
      word = m_words[toWordIndex];
      while (word != static_cast<TWord>(0))
      {
         const size_t bitIndex = CountTrailingZeros(word);
         if (bitIndex >= toBitIndex)
         {
            return;
         }
         func(toWordIndex * BitsPerWord + bitIndex);
         word &= ~(static_cast<TWord>(1u) << bitIndex);
      }
   }
}


template<size_t TSize, typename TWord>
void FixedBitset<TSize, TWord>::Clear()
{
   memset(m_words, 0, sizeof(m_words));
}


} // namespace slotmap

