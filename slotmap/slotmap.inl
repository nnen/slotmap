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



namespace slotmap {


//////////////////////////////////////////////////////////////////////////
#define SLOTMAP_CHUNK_INVARIANTS(chunk_) \
   assert( \
      (((chunk_)->m_firstFreeSlot < 0) && ((chunk_)->m_lastFreeSlot < 0)) || \
      (((chunk_)->m_firstFreeSlot >= 0) && ((chunk_)->m_lastFreeSlot >= 0)) \
   );


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::FixedSlotMapStorage()
{
   m_firstFreeSlot = 0;
   for (SizeType i = 0; i < TCapacity - 1; ++i)
   {
      m_slots[i].m_nextFreeSlot = i + 1;
   }
   m_slots[TCapacity - 1].m_nextFreeSlot = -1;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::FixedSlotMapStorage(const FixedSlotMapStorage& other)
   : m_size(other.m_size)
   , m_firstFreeSlot(other.m_firstFreeSlot)
   , m_liveBits(other.m_liveBits)
{
   for (size_t i = 0; i < StaticCapacity; ++i)
   {
      m_generations[i] = other.m_generations[i];
      if (other.m_liveBits[i])
      {
         TValue* ptr = m_slots[i].GetPtr();
         const TValue* otherPtr = other.m_slots[i].GetPtr();
         new (ptr) TValue(*otherPtr);
      }
      else
      {
         m_slots[i].m_nextFreeSlot = other.m_slots[i].m_nextFreeSlot;
      }
   }
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::FixedSlotMapStorage(FixedSlotMapStorage&& other)
   : m_size(other.m_size)
   , m_firstFreeSlot(other.m_firstFreeSlot)
   , m_liveBits(std::move(other.m_liveBits))
{
   for (SizeType i = 0; i < TCapacity; ++i)
   {
      m_generations[i] = other.m_generations[i];
      if (m_liveBits[i])
      {
         TValue* ptr = m_slots[i].GetPtr();
         TValue* otherPtr = other.m_slots[i].GetPtr();
         new (ptr) TValue(std::move(*otherPtr));
         otherPtr->~TValue();
      }
      else
      {
         m_slots[i].m_nextFreeSlot = other.m_slots[i].m_nextFreeSlot;
      }
      other.m_slots[i].m_nextFreeSlot = i + 1;
   }

   other.m_size = 0;
   other.m_firstFreeSlot = 0;
   other.m_liveBits.reset();
   other.m_slots[TCapacity - 1].m_nextFreeSlot = -1;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::~FixedSlotMapStorage()
{
   Clear();
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>& FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::operator=(FixedSlotMapStorage&& other)
{
   Clear();

   for (size_t i = 0; i < StaticCapacity; ++i)
   {
      m_generations[i] = other.m_generations[i];
      if (other.m_liveBits[i])
      {
         TValue* ptr = m_slots[i].GetPtr();
         TValue* otherPtr = other.m_slots[i].GetPtr();
         new (ptr) TValue(std::move(*otherPtr));
         otherPtr->~TValue();
      }
      else
      {
         m_slots[i].m_nextFreeSlot = other.m_slots[i].m_nextFreeSlot;
      }
      other.m_slots[i].m_nextFreeSlot = i + 1;
   }

   m_size = other.m_size;
   m_firstFreeSlot = other.m_firstFreeSlot;
   m_liveBits = std::move(other.m_liveBits);

   other.m_size = 0;
   other.m_firstFreeSlot = 0;
   other.m_liveBits.reset();
   other.m_slots[TCapacity - 1].m_nextFreeSlot = -1;

   return *this;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t Capacity,
   typename TBitset>
template<typename TSelf>
auto FixedSlotMapStorage<TValue, TKey, Capacity, TBitset>::GetPtrTpl(TSelf self, TKey key)
{
   using ReturnType = decltype(self->m_slots[0].GetPtr());

   const TKey slotIndex = key & SlotIndexMask;
   if ((slotIndex >= StaticCapacity) || !self->m_liveBits[slotIndex])
   {
      return static_cast<ReturnType>(nullptr);
   }

   const GenerationType generation = static_cast<GenerationType>((key >> GenerationShift) & GenerationMask);
   if (self->m_generations[slotIndex] != generation)
   {
      return static_cast<ReturnType>(nullptr);
   }

   return self->m_slots[slotIndex].GetPtr();
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
bool FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::FindNextKey(TKey& key) const
{
   TKey slotIndex = static_cast<TKey>(TBitset::template FindNextBitSet(m_liveBits, key & SlotIndexMask));

   if (slotIndex >= TCapacity)
   {
      return false;
   }

   key = (static_cast<TKey>(m_generations[slotIndex]) << GenerationShift) | slotIndex;
   return true;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
TKey FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::IncrementKey(TKey key) const
{
   return key + 1;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
template<typename TFunc>
void FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::ForEachSlot(TFunc func) const
{
   m_liveBits.ForEachSetBit([&](size_t index)
   {
      const TKey key = (static_cast<TKey>(m_generations[index]) << GenerationShift) | static_cast<TKey>(index);
      func(key, *m_slots[index].GetPtr());
   });
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
TKey FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::AllocateSlot(TValue*& outPtr)
{
   if (m_firstFreeSlot < 0)
   {
      return InvalidKey;
   }
   
   const SizeType slotIndex = m_firstFreeSlot;
   assert(!m_liveBits[slotIndex]);
   m_firstFreeSlot = m_slots[slotIndex].m_nextFreeSlot;

   outPtr = m_slots[slotIndex].GetPtr();
   ++m_generations[slotIndex];
   m_liveBits.set(slotIndex);

   ++m_size;

   return (static_cast<TKey>(m_generations[slotIndex]) << GenerationShift) | static_cast<TKey>(slotIndex);
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
bool FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::FreeSlot(KeyType key)
{
   const SizeType slotIndex = static_cast<SizeType>(key & SlotIndexMask);
   if ((slotIndex >= TCapacity) || !m_liveBits[slotIndex])
   {
      return false;
   }

   const GenerationType generation = static_cast<GenerationType>((key >> GenerationShift) & GenerationMask);
   if (m_generations[slotIndex] != generation)
   {
      return false;
   }

   if constexpr (!std::is_trivially_destructible_v<TValue>)
   {
      m_slots[slotIndex].GetPtr()->~TValue();
   }
   
   m_slots[slotIndex].m_nextFreeSlot = m_firstFreeSlot;
   m_firstFreeSlot = static_cast<IndexType>(slotIndex);

   m_liveBits.reset(slotIndex);
   assert(m_size > 0);
   --m_size;
   
   return true;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
void FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::Swap(FixedSlotMapStorage& other)
{
   FixedSlotMapStorage tmp(std::move(other));
   other = std::move(*this);
   *this = std::move(tmp);
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity,
   typename TBitset>
void FixedSlotMapStorage<TValue, TKey, TCapacity, TBitset>::Clear()
{
   for (size_t slotIndex = 0; slotIndex < TCapacity; ++slotIndex)
   {
      if (m_liveBits[slotIndex])
      {
         if constexpr (!std::is_trivially_destructible_v<TValue>)
         {
            m_slots[slotIndex].GetPtr()->~TValue();
         }

         m_slots[slotIndex].m_nextFreeSlot = m_firstFreeSlot;
         m_firstFreeSlot = static_cast<IndexType>(slotIndex);

         m_liveBits.reset(slotIndex);
      }
   }
   
   m_size = 0;
}


//////////////////////////////////////////////////////////////////////////
template<size_t TSlotCount, typename TValue, typename TIndexType, typename TGenerationType, typename TBitsetTraits>
ChunkTpl<TSlotCount, TValue, TIndexType, TGenerationType, TBitsetTraits>::ChunkTpl(const ChunkTpl& other) 
   : m_nextFreeChunk(other.m_nextFreeChunk)
   , m_firstFreeSlot(other.m_firstFreeSlot)
   , m_lastFreeSlot(other.m_lastFreeSlot)
   , m_liveBits(other.m_liveBits)
{
   for (size_t i = 0; i < TSlotCount; ++i)
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


//////////////////////////////////////////////////////////////////////////
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
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::ChunkedSlotMapStorage(ChunkedSlotMapStorage&& other)
   : m_size(other.m_size)
   , m_firstFreeChunk(other.m_firstFreeChunk)
   , m_chunks(std::move(other.m_chunks))
{
   other.m_size = 0;
   other.m_firstFreeChunk = -1;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>& ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::operator=(ChunkedSlotMapStorage&& other)
{
   m_size = other.m_size;
   m_firstFreeChunk = other.m_firstFreeChunk;
   m_chunks = std::move(other.m_chunks);

   other.m_size = 0;
   other.m_firstFreeChunk = -1;

   return *this;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
TValue* ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::GetPtr(TKey key) const
{
   const KeyType chunkIndex = key & ChunkIndexMask;
   if (chunkIndex >= m_chunks.size())
   {
      return nullptr;
   }
   
   Chunk& chunk = *m_chunks[chunkIndex];
   const KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;
   if ((slotIndex >= ChunkSlots) || !chunk.m_liveBits[slotIndex])
   {
      return nullptr;
   }

   const GenerationType generation = (key >> GenerationShift) & GenerationMask;
   if (chunk.m_generations[slotIndex] != generation)
   {
      return nullptr;
   }

   return chunk.m_slots[slotIndex].GetPtr();
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
bool ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::FindNextKey(TKey& key) const
{
   KeyType chunkIndex = key & ChunkIndexMask;
   KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;

   for (; chunkIndex < m_chunks.size(); ++chunkIndex)
   {
      Chunk& chunk = *m_chunks[chunkIndex];
      slotIndex = static_cast<KeyType>(TBitsetTraits::template FindNextBitSet(chunk.m_liveBits, slotIndex));

      if (slotIndex < ChunkSlots)
      {
         key = (static_cast<KeyType>(chunk.m_generations[slotIndex]) << GenerationShift) |
            (static_cast<KeyType>(slotIndex) << SlotIndexShift) |
            chunkIndex;
         return true;
      }

      slotIndex = 0;
   }
   
   return false;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
TKey ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::IncrementKey(TKey key) const
{
   const KeyType chunkIndex = key & ChunkIndexMask;
   KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;

   ++slotIndex;

   if (slotIndex < ChunkSlots)
   {
      return (slotIndex << SlotIndexShift) | chunkIndex;
   }
   
   return chunkIndex + 1;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
template<typename TFunc>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::ForEachSlot(TFunc func) const
{
   for (size_t chunkIndex = 0; chunkIndex < m_chunks.size(); ++chunkIndex)
   {
      const Chunk* chunk = m_chunks[chunkIndex];
      chunk->m_liveBits.ForEachSetBit([&](size_t slotIndex)
      {
         const TKey key = (static_cast<KeyType>(chunk->m_generations[slotIndex]) << GenerationShift) |
            (static_cast<KeyType>(slotIndex) << SlotIndexShift) |
            static_cast<KeyType>(chunkIndex);
         func(key, *chunk->m_slots[slotIndex].GetPtr());
      });
   }
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::AllocateChunk()
{
   Chunk* newChunk = new Chunk();

   for (size_t i = 0; i < ChunkSlots - 1; ++i)
   {
      newChunk->m_slots[i].m_nextFreeSlot = i + 1;
   }
   newChunk->m_slots[ChunkSlots - 1].m_nextFreeSlot = -1;
   newChunk->m_firstFreeSlot = 0;
   newChunk->m_lastFreeSlot = ChunkSlots - 1;
   
   newChunk->m_nextFreeChunk = m_firstFreeChunk;
   m_firstFreeChunk = static_cast<IndexType>(m_chunks.size());

   m_chunks.push_back(newChunk);
   
   SLOTMAP_CHUNK_INVARIANTS(newChunk);
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
TKey ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::AllocateSlot(ValueType*& outPtr)
{
   if (m_firstFreeChunk < 0)
   {
      AllocateChunk();
   }
   assert(m_firstFreeChunk >= 0);

   const KeyType chunkIndex = static_cast<KeyType>(m_firstFreeChunk);
   Chunk* chunk = m_chunks[chunkIndex];
   assert(chunk->m_firstFreeSlot >= 0);

   const SizeType slotIndex = chunk->m_firstFreeSlot;
   Slot* const slot = chunk->m_slots + slotIndex;
   outPtr = slot->GetPtr();

   chunk->m_firstFreeSlot = slot->m_nextFreeSlot;
   if (chunk->m_firstFreeSlot < 0)
   {
      chunk->m_lastFreeSlot = -1;
      m_firstFreeChunk = chunk->m_nextFreeChunk;
   }

   ++chunk->m_generations[slotIndex];
   assert(!chunk->m_liveBits[slotIndex]);
   chunk->m_liveBits.set(slotIndex);

   ++m_size;
   
   SLOTMAP_CHUNK_INVARIANTS(chunk);

   return (static_cast<KeyType>(chunk->m_generations[slotIndex]) << GenerationShift) |
      (static_cast<KeyType>(slotIndex) << SlotIndexShift) |
      chunkIndex;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
bool ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::FreeSlot(KeyType key)
{
   const KeyType chunkIndex = key & ChunkIndexMask;
   if (chunkIndex >= m_chunks.size())
   {
      return false;
   }

   Chunk& chunk = *m_chunks[chunkIndex];
   const KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;
   if ((slotIndex >= ChunkSlots) || !chunk.m_liveBits[slotIndex])
   {
      return false;
   }

   const GenerationType generation = (key >> GenerationShift) & GenerationMask;
   if (chunk.m_generations[slotIndex] != generation)
   {
      return false;
   }

   Slot* const slot = chunk.m_slots + slotIndex;
   slot->GetPtr()->~TValue();

   slot->m_nextFreeSlot = chunk.m_firstFreeSlot;
   const bool isChunkInFreeList = (chunk.m_firstFreeSlot >= 0);
   chunk.m_firstFreeSlot = slotIndex;
   if (!isChunkInFreeList)
   {
      chunk.m_lastFreeSlot = slotIndex;
      chunk.m_nextFreeChunk = m_firstFreeChunk;
      m_firstFreeChunk = chunkIndex;
   }

   assert(chunk.m_liveBits[slotIndex]);
   chunk.m_liveBits.reset(slotIndex);
   assert(m_size > 0);
   --m_size;

   SLOTMAP_CHUNK_INVARIANTS(&chunk);
   
   return true;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::FreeSlotByIndex(IndexType chunkIndex, IndexType slotIndex)
{
   Chunk* const chunk = m_chunks[chunkIndex];

   assert(chunk->m_liveBits[slotIndex]);
   chunk->m_liveBits.reset(slotIndex);

   Slot* const slot = chunk->m_slots + slotIndex;
   if constexpr (!std::is_trivially_destructible_v<TValue>)
   {
      slot->GetPtr()->~TValue();
   }

   slot->m_nextFreeSlot = -1;
   if (chunk->m_lastFreeSlot < 0)
   {
      assert(chunk->m_firstFreeSlot < 0);
      chunk->m_firstFreeSlot = slotIndex;
   }
   else
   {
      assert(chunk->m_firstFreeSlot >= 0);
      chunk->m_slots[chunk->m_lastFreeSlot].m_nextFreeSlot = slotIndex;
   }
   chunk->m_lastFreeSlot = slotIndex;
   
   assert(m_size > 0);
   --m_size;
   
   SLOTMAP_CHUNK_INVARIANTS(chunk);
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::Swap(ChunkedSlotMapStorage& other)
{
   std::swap(m_size, other.m_size);
   std::swap(m_firstFreeChunk, other.m_firstFreeChunk);
   std::swap(m_chunks, other.m_chunks);
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator,
   typename TBitsetTraits>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator, TBitsetTraits>::Clear()
{
   if (m_chunks.size() == 0)
   {
      return;
   }

   m_firstFreeChunk = 0;
   m_chunks.back()->m_nextFreeChunk = -1;

   for (size_t i = 0; i < m_chunks.size() - 1; ++i)
   {
      m_chunks[i]->m_nextFreeChunk = i + 1;
   }

   for (IndexType chunkIndex = 0; chunkIndex < static_cast<IndexType>(m_chunks.size()); ++chunkIndex)
   {
      Chunk* const chunk = m_chunks[chunkIndex];

      for (IndexType slotIndex = 0; slotIndex < ChunkSlots; ++slotIndex)
      {
         if (chunk->m_liveBits[slotIndex])
         {
            FreeSlotByIndex(chunkIndex, slotIndex);
         }
      }
   }

   assert(m_size == 0);
}


//////////////////////////////////////////////////////////////////////////
template<typename TValue, typename TKey, typename TStorage>
template<typename... TArgs>
TKey SlotMap<TValue, TKey, TStorage>::Emplace(TArgs&&... args)
{
   TValue* ptr = nullptr;
   const TKey key = m_storage.AllocateSlot(ptr);

   if (!ptr)
   {
      return InvalidKey;
   }

   new (ptr) TValue(std::forward<TArgs>(args)...);

   return key;
}


}