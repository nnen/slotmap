#pragma once
#include <cstdint>
#include <climits>
#include <type_traits>
#include <bitset>
#include <cassert>

#ifdef _MSC_VER
#include <intrin.h>
#endif


namespace slotmap {


//////////////////////////////////////////////////////////////////////////
inline int CountTrailingZeros(uint64_t x)
{
#ifdef _MSC_VER
   unsigned long r;
   _BitScanForward64(&r, x);
   return static_cast<int>(r);
#else
   return __builtin_ctzll(x);
#endif
}


//////////////////////////////////////////////////////////////////////////
/**
 * This is a fixed bitset implementation very similar to `std::bitset`, but with a few improvements.
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
   static_assert(std::is_unsigned_v<TWord>);

   static constexpr size_t StaticSize = TSize;

   class Iterator
   {
   public:
   private:
      size_t m_index = 0;
      TWord* m_word = nullptr;
   };

   FixedBitset();
   FixedBitset(const FixedBitset& other);
   FixedBitset(FixedBitset&& other);

   FixedBitset& operator=(const FixedBitset& other);
   FixedBitset& operator=(FixedBitset&& other);

   inline bool operator[](size_t index) const { return Get(index); }

   bool Get(size_t index) const;
   void Set(size_t index);
   void Unset(size_t index);
   void Set(size_t index, bool value);

   inline size_t FindNextBitSet(size_t start) const;

   template<typename TFunc>
   void ForEachSetBit(TFunc func) const;
   
   void Clear();
   
   // STL-compatibility functions
   inline size_t size() const { return StaticSize; }
   inline bool test(size_t index) const { return Get(index); }
   inline void set(size_t index) { Set(index); }
   inline void reset(size_t index) { Unset(index); }
   inline void reset() { Clear(); }

private:
   static constexpr size_t BitsPerWord = sizeof(TWord) * CHAR_BIT;
   static constexpr size_t NumWords = (StaticSize + BitsPerWord - 1) / BitsPerWord;
   static constexpr size_t BitIndexMask = BitsPerWord - 1;

   static constexpr size_t GetWordIndex(size_t index) { return index / BitsPerWord; }
   static constexpr size_t GetBitIndex(size_t index) { return index & BitIndexMask; }

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
bool FixedBitset<TSize, TWord>::Get(size_t index) const
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
void FixedBitset<TSize, TWord>::Clear()
{
   memset(m_words, 0, sizeof(m_words));
}


} // namespace slotmap

