namespace slotmap {


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
   m_slots[TCapacity - 1].m_nextFreeSlot = InvalidKey;
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
TKey FixedSlotMapStorage<TValue, TKey, TCapacity>::AllocateSlot(TValue*& outPtr)
{
   assert(m_firstFreeSlot >= 0);

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

   m_slots[slotIndex].GetPtr()->~TValue();
   
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
void ChunkedSlotMapStorage<TValue, TKey, MaxChunkSize, TAllocator>::AllocateChunk()
{
   Chunk* newChunk = new Chunk();

   for (size_t i = 0; i < ChunkSize - 1; ++i)
   {
      newChunk->m_slots[i].m_nextFreeSlot = i + 1;
   }
   newChunk->m_slots[ChunkSize - 1].m_nextFreeSlot = -1;
   newChunk->m_firstFreeSlot = 0;

   newChunk->m_nextFreeChunk = m_firstFreeChunk;
   m_firstFreeChunk = static_cast<IndexType>(m_chunks.size());

   m_chunks.push_back(newChunk);
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
      m_firstFreeChunk = chunk->m_nextFreeChunk;
   }

   ++chunk->m_generations[slotIndex];
   assert(!chunk->m_liveBits[slotIndex]);
   chunk->m_liveBits.set(slotIndex);

   ++m_size;

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
      chunk.m_nextFreeChunk = m_firstFreeChunk;
      m_firstFreeChunk = chunkIndex;
   }

   assert(chunk.m_liveBits[slotIndex]);
   chunk.m_liveBits.reset(slotIndex);
   assert(m_size > 0);
   --m_size;

   return true;
}


}