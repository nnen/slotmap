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
   size_t TCapacity>
FixedSlotMapStorage<TValue, TKey, TCapacity>::FixedSlotMapStorage()
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
   size_t TCapacity>
FixedSlotMapStorage<TValue, TKey, TCapacity>::FixedSlotMapStorage(FixedSlotMapStorage&& other)
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
   size_t TCapacity>
FixedSlotMapStorage<TValue, TKey, TCapacity>::~FixedSlotMapStorage()
{
   Clear();
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity>
FixedSlotMapStorage<TValue, TKey, TCapacity>& FixedSlotMapStorage<TValue, TKey, TCapacity>::operator=(FixedSlotMapStorage&& other)
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
   size_t Capacity>
template<typename TSelf>
auto FixedSlotMapStorage<TValue, TKey, Capacity>::GetPtrTpl(TSelf self, TKey key)
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
   size_t TCapacity>
bool FixedSlotMapStorage<TValue, TKey, TCapacity>::FindNextKey(TKey& key) const
{
   TKey slotIndex = key & SlotIndexMask;

   for (; slotIndex < StaticCapacity; ++slotIndex)
   {
      if (m_liveBits[slotIndex])
      {
         key = (static_cast<TKey>(m_generations[slotIndex]) << GenerationShift) | slotIndex;
         return true;
      }
   }
   
   return false;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity>
TKey FixedSlotMapStorage<TValue, TKey, TCapacity>::IncrementKey(TKey key) const
{
   return key + 1;
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity>
TKey FixedSlotMapStorage<TValue, TKey, TCapacity>::AllocateSlot(TValue*& outPtr)
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
   size_t TCapacity>
bool FixedSlotMapStorage<TValue, TKey, TCapacity>::FreeSlot(KeyType key)
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
   size_t TCapacity>
void FixedSlotMapStorage<TValue, TKey, TCapacity>::Swap(FixedSlotMapStorage& other)
{
   FixedSlotMapStorage tmp(std::move(other));
   other = std::move(*this);
   *this = std::move(tmp);
}


//////////////////////////////////////////////////////////////////////////
template<
   typename TValue,
   typename TKey,
   size_t TCapacity>
void FixedSlotMapStorage<TValue, TKey, TCapacity>::Clear()
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
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize,
   typename TAllocator>
ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::ChunkedSlotMapStorage(ChunkedSlotMapStorage&& other)
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
   typename TAllocator>
ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>& ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::operator=(ChunkedSlotMapStorage&& other)
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
   typename TAllocator>
TValue* ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::GetPtr(TKey key) const
{
   const KeyType chunkIndex = key & ChunkIndexMask;
   if (chunkIndex >= m_chunks.size())
   {
      return nullptr;
   }
   
   Chunk& chunk = *m_chunks[chunkIndex];
   const KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;
   if ((slotIndex >= ChunkSize) || !chunk.m_liveBits[slotIndex])
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
   typename TAllocator>
bool ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::FindNextKey(TKey& key) const
{
   KeyType chunkIndex = key & ChunkIndexMask;
   KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;

   for (; chunkIndex < m_chunks.size(); ++chunkIndex)
   {
      Chunk& chunk = *m_chunks[chunkIndex];

      for (; slotIndex < ChunkSize; ++slotIndex)
      {
         if (chunk.m_liveBits[slotIndex])
         {
            key = (static_cast<KeyType>(chunk.m_generations[slotIndex]) << GenerationShift) |
               (static_cast<KeyType>(slotIndex) << SlotIndexShift) |
               chunkIndex;
            return true;
         }
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
   typename TAllocator>
TKey ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::IncrementKey(TKey key) const
{
   const KeyType chunkIndex = key & ChunkIndexMask;
   KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;

   ++slotIndex;

   if (slotIndex < ChunkSize)
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
   typename TAllocator>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::AllocateChunk()
{
   Chunk* newChunk = new Chunk();

   for (size_t i = 0; i < ChunkSize - 1; ++i)
   {
      newChunk->m_slots[i].m_nextFreeSlot = i + 1;
   }
   newChunk->m_slots[ChunkSize - 1].m_nextFreeSlot = -1;
   newChunk->m_firstFreeSlot = 0;
   newChunk->m_lastFreeSlot = ChunkSize - 1;
   
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
   typename TAllocator>
TKey ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::AllocateSlot(ValueType*& outPtr)
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
   typename TAllocator>
bool ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::FreeSlot(KeyType key)
{
   const KeyType chunkIndex = key & ChunkIndexMask;
   if (chunkIndex >= m_chunks.size())
   {
      return false;
   }

   Chunk& chunk = *m_chunks[chunkIndex];
   const KeyType slotIndex = (key >> SlotIndexShift) & SlotIndexMask;
   if ((slotIndex >= ChunkSize) || !chunk.m_liveBits[slotIndex])
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
   typename TAllocator>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::FreeSlotByIndex(IndexType chunkIndex, IndexType slotIndex)
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
   typename TAllocator>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::Swap(ChunkedSlotMapStorage& other)
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
   typename TAllocator>
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::Clear()
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

      for (IndexType slotIndex = 0; slotIndex < ChunkSize; ++slotIndex)
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