// Copyright (c) 2024, Jan Milik (jan.milik@gmail.com).
// 
// All rights reserved.
//
// MIT License
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <type_traits>
#include <climits>
#include <vector>
#include <bitset>
#include <memory>
#include <cassert>

#include "bitset.h"


namespace slotmap {


//////////////////////////////////////////////////////////////////////////
constexpr int GetIndexBitSize(int arraySize)
{
   return ((arraySize <= 2) ? 1 : 1 + GetIndexBitSize(arraySize >> 1));
}


//////////////////////////////////////////////////////////////////////////
/**
 * Fixed-capacity statically allocated SlotMap storage.
 */
template<
   typename TValue,
   typename TKey,
   size_t TCapacity = 1024,
   typename TBitsetTraits = FixedBitSetTraits<>>
struct FixedSlotMapStorage
{
   using ValueType = TValue;
   using KeyType = TKey;
   using GenerationType = uint8_t;

   using SizeType = size_t;
   using IndexType = ptrdiff_t;

   using BitsetType = typename TBitsetTraits::template BitsetType<TCapacity>;

   static_assert(std::is_unsigned_v<KeyType>);
   static_assert(sizeof(GenerationType) < sizeof(KeyType));

   static constexpr size_t StaticCapacity = TCapacity;
   static constexpr KeyType InvalidKey = static_cast<KeyType>(-1);

   static constexpr KeyType GenerationBitSize = sizeof(GenerationType) * CHAR_BIT;
   static constexpr KeyType SlotIndexBitSize = (sizeof(TKey) * CHAR_BIT) - GenerationBitSize;

   static constexpr KeyType MaxSlotCount = (static_cast<KeyType>(1) << SlotIndexBitSize);
   static constexpr KeyType SlotIndexMask = MaxSlotCount - 1;

   static constexpr KeyType GenerationShift = SlotIndexBitSize;
   static constexpr KeyType GenerationMask = (1 << GenerationBitSize) - 1;

   static_assert(MaxSlotCount > TCapacity, "The key type is too small for the given capacity.");

   struct Slot
   { 
      union
      {
         alignas(TValue) uint8_t m_storage[sizeof(TValue)];
         IndexType m_nextFreeSlot;
      };

      inline TValue* GetPtr() { return reinterpret_cast<TValue*>(m_storage); }
      inline const TValue* GetPtr() const { return reinterpret_cast<const TValue*>(m_storage); }
   };

   class Iterator
   {
   };

   FixedSlotMapStorage();
   FixedSlotMapStorage(const FixedSlotMapStorage&);
   FixedSlotMapStorage(FixedSlotMapStorage&& other);

   ~FixedSlotMapStorage();

   FixedSlotMapStorage& operator=(const FixedSlotMapStorage&) = delete;
   FixedSlotMapStorage& operator=(FixedSlotMapStorage&&);

   inline SizeType Size() const { return m_size; }
   inline SizeType Capacity() const { return Capacity; }

   template<typename TSelf>
   static inline auto GetPtrTpl(TSelf self, TKey key);
   
   inline TValue* GetPtr(TKey key) { return GetPtrTpl(this, key); }
   inline const TValue* GetPtr(TKey key) const { return GetPtrTpl(this, key); }
   
   bool FindNextKey(TKey& key) const;
   TKey IncrementKey(TKey key) const;

   template<typename TFunc>
   void ForEachSlot(TFunc func) const;

   KeyType AllocateSlot(ValueType*& outPtr);
   bool FreeSlot(KeyType key);

   void Swap(FixedSlotMapStorage& other);
   void Clear();
   
   SizeType m_size = 0;
   IndexType m_firstFreeSlot = -1;
   BitsetType m_liveBits;
   GenerationType m_generations[TCapacity];
   Slot m_slots[TCapacity];
};


//////////////////////////////////////////////////////////////////////////
constexpr size_t DefaultMaxChunkSize = 2048;


//////////////////////////////////////////////////////////////////////////
/**
 * Dynamically allocated SlotMap storage implemented as a chunked vector.
 */
template<
   typename TValue,
   typename TKey = uint32_t,
   size_t MaxChunkSize = DefaultMaxChunkSize,
   typename TAllocator = std::allocator<TValue>,
   typename TBitsetTraits = FixedBitSetTraits<>>
struct ChunkedSlotMapStorage
{
   using ValueType = TValue;
   using KeyType = TKey;
   using GenerationType = uint8_t;

   using SizeType = size_t;
   using IndexType = ptrdiff_t;

   static_assert(std::is_unsigned_v<KeyType>);
   static_assert(sizeof(KeyType) > sizeof(GenerationType));

   static constexpr KeyType InvalidKey = static_cast<KeyType>(-1);

   static constexpr size_t MaxChunkItems = std::min(1 << CHAR_BIT, static_cast<int>(MaxChunkSize / sizeof(ValueType)));
   static constexpr int GenerationBitSize = sizeof(GenerationType) * CHAR_BIT;
   static constexpr int SlotIndexBitSize = std::min(GetIndexBitSize(MaxChunkItems), static_cast<int>(sizeof(KeyType) * CHAR_BIT - GenerationBitSize - 1));
   static constexpr int ChunkIndexBitSize = (sizeof(TKey) * CHAR_BIT) - GenerationBitSize - SlotIndexBitSize;
   
   static constexpr KeyType ChunkSize = (static_cast<KeyType>(1) << SlotIndexBitSize);
   
   static constexpr KeyType ChunkIndexMask = (static_cast<KeyType>(1) << ChunkIndexBitSize) - 1;
   static constexpr KeyType MaxChunkCount = ChunkIndexMask + 1;
   
   static constexpr KeyType SlotIndexShift = ChunkIndexBitSize;
   static constexpr KeyType SlotIndexMask = (static_cast<KeyType>(1) << SlotIndexBitSize) - 1;
   
   static constexpr KeyType GenerationShift = ChunkIndexBitSize + SlotIndexBitSize;
   static constexpr KeyType GenerationMask = (static_cast<KeyType>(1) << GenerationBitSize) - 1;

   using BitsetType = typename TBitsetTraits::template BitsetType<ChunkSize>;

   static_assert(SlotIndexBitSize > 0);
   static_assert(ChunkIndexBitSize > 0);
   
   struct Slot
   {
      union
      {
         alignas(TValue) uint8_t m_storage[sizeof(TValue)];
         IndexType m_nextFreeSlot;
      };

      inline TValue* GetPtr() { return reinterpret_cast<TValue*>(m_storage); }
      inline const TValue* GetPtr() const { return reinterpret_cast<const TValue*>(m_storage); }
   };

   struct Chunk
   {
      Chunk() = default;
      Chunk(const Chunk& other);

      IndexType m_nextFreeChunk = -1;
      IndexType m_firstFreeSlot = -1;
      IndexType m_lastFreeSlot = -1;

      BitsetType m_liveBits;
      GenerationType m_generations[ChunkSize];
      Slot m_slots[ChunkSize];
   };

   class Iterator
   {
   public:
      inline Iterator(ChunkedSlotMapStorage* storage) : m_storage(storage) {}

      bool operator==(const Iterator& other) const;
      bool operator!=(const Iterator& other) const;

      Iterator& operator++();
      Iterator& operator++(int);

   private:
      ChunkedSlotMapStorage* m_storage = nullptr;
      KeyType m_chunkIndex = 0;
      KeyType m_slotIndex = 0;
   };

   ChunkedSlotMapStorage() = default;
   ChunkedSlotMapStorage(const ChunkedSlotMapStorage&);
   ChunkedSlotMapStorage(ChunkedSlotMapStorage&& other);

   inline ~ChunkedSlotMapStorage() { Clear(); }

   ChunkedSlotMapStorage& operator=(const ChunkedSlotMapStorage&) = delete;
   ChunkedSlotMapStorage& operator=(ChunkedSlotMapStorage&& other);

   inline SizeType Size() const { return m_size; }
   inline SizeType Capacity() const { return m_chunks.size() * ChunkSize; }

   TValue* GetPtr(TKey key) const;

   bool FindNextKey(TKey& key) const;
   TKey IncrementKey(TKey key) const;

   template<typename TFunc>
   void ForEachSlot(TFunc func) const;

   void AllocateChunk();
   KeyType AllocateSlot(ValueType*& outPtr);
   bool FreeSlot(KeyType key);
   void FreeSlotByIndex(IndexType chunkIndex, IndexType slotIndex);
   
   void Swap(ChunkedSlotMapStorage& other);
   void Clear();

   using ChunkAllocator = typename std::allocator_traits<TAllocator>::template rebind_alloc<Chunk>;
   using ChunkPtrAllocator = typename std::allocator_traits<TAllocator>::template rebind_alloc<Chunk*>;

   SizeType m_size = 0;
   IndexType m_firstFreeChunk = -1;
   std::vector<Chunk*, ChunkPtrAllocator> m_chunks;
};


template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::Chunk::Chunk(const Chunk& other)
   : m_nextFreeChunk(other.m_nextFreeChunk)
   , m_firstFreeSlot(other.m_firstFreeSlot)
   , m_lastFreeSlot(other.m_lastFreeSlot)
   , m_liveBits(other.m_liveBits)
{
   for (size_t i = 0; i < ChunkSize; ++i)
   {
      m_generations[i] = other.m_generations[i];
      if (other.m_liveBits[i])
      {
         TValue* const ptr = m_slots[i].GetPtr();
         const TValue* const otherPtr = other.m_slots[i].GetPtr();
         new (ptr) TValue(*otherPtr);
      }
      else
      {
         m_slots[i].m_nextFreeSlot = other.m_slots[i].m_nextFreeSlot;
      }
   }
}


template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::ChunkedSlotMapStorage(const ChunkedSlotMapStorage& other)
   : m_size(other.m_size)
   , m_firstFreeChunk(other.m_firstFreeChunk)
{
   m_chunks.resize(other.m_chunks.size());
   
   for (size_t i = 0; i < other.m_chunks.size(); ++i)
   {
      m_chunks[i] = new Chunk(*other.m_chunks[i]);
   }
}


//////////////////////////////////////////////////////////////////////////
template<typename TValue, typename TKey = uint32_t, typename TStorage = ChunkedSlotMapStorage<TValue, TKey>>
class SlotMap
{
public:
   using ValueType = typename TStorage::ValueType;
   using KeyType = typename TStorage::KeyType;
   using SizeType = typename TStorage::SizeType;
   using Iterator = typename TStorage::Iterator;

   static constexpr KeyType InvalidKey = TStorage::InvalidKey;

   SlotMap() = default;
   inline SlotMap(const SlotMap& other) : m_storage(other.m_storage) {}
   inline SlotMap(SlotMap&& other) : m_storage(std::move(other.m_storage)) {}
   
   inline SlotMap& operator=(const SlotMap& other) { m_storage = other.m_storage; return *this; }
   inline SlotMap& operator=(SlotMap&& other) { m_storage = std::move(other.m_storage); return *this; }
   
   inline SizeType Size() const { return m_storage.Size(); }
   inline SizeType Capacity() const { return m_storage.Capacity(); }
   bool Reserve(SizeType capacity);

   template<typename... TArgs>
   TKey Emplace(TArgs&&... args);

   inline bool Erase(TKey key) { return m_storage.FreeSlot(key); }
   inline void Swap(SlotMap& other) { m_storage.Swap(other.m_storage); }
   inline void Clear() { m_storage.Clear(); }

   inline TValue* GetPtr(TKey key) { return m_storage.GetPtr(key); }
   inline const TValue* GetPtr(TKey key) const { return m_storage.GetPtr(key); }

   inline bool FindNextKey(TKey& key) const { return m_storage.FindNextKey(key); }
   inline TKey IncrementKey(TKey key) const { return m_storage.IncrementKey(key); }

   template<typename TFunc>
   inline void ForEachSlot(TFunc func) const { m_storage.ForEachSlot(func); }

private:
   TStorage m_storage;
};


template<typename TValue, size_t Capacity, typename TKey = uint32_t>
using FixedSlotMap = SlotMap<TValue, TKey, FixedSlotMapStorage<TValue, TKey, Capacity>>;


} // namespace slotmap


#include "slotmap.inl"

