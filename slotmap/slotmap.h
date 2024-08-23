#pragma once

#include <type_traits>
#include <climits>
#include <vector>
#include <bitset>
#include <memory>
#include <cassert>


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
   size_t TCapacity = 1024>
struct FixedSlotMapStorage
{
   using ValueType = TValue;
   using KeyType = TKey;
   using GenerationType = uint8_t;

   using SizeType = size_t;
   using IndexType = ptrdiff_t;

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
   FixedSlotMapStorage(const FixedSlotMapStorage&) = delete;
   FixedSlotMapStorage(FixedSlotMapStorage&& other);

   ~FixedSlotMapStorage();

   inline SizeType Size() const { return m_size; }
   inline SizeType Capacity() const { return Capacity; }

   template<typename TSelf>
   static inline auto GetPtrTpl(TSelf self, TKey key);
   
   inline TValue* GetPtr(TKey key) { return GetPtrTpl(this, key); }
   inline const TValue* GetPtr(TKey key) const { return GetPtrTpl(this, key); }
   
   bool FindNextKey(TKey& key) const;
   TKey IncrementKey(TKey key) const;
   
   KeyType AllocateSlot(ValueType*& outPtr);
   bool FreeSlot(KeyType key);

   void Clear();
   
   SizeType m_size = 0;
   IndexType m_firstFreeSlot = -1;
   std::bitset<TCapacity> m_liveBits;
   GenerationType m_generations[TCapacity];
   Slot m_slots[TCapacity];
};


//////////////////////////////////////////////////////////////////////////
/**
 * Dynamically allocated SlotMap storage implemented as a chunked vector.
 */
template<
   typename TValue,
   typename TKey,
   size_t MaxChunkSize = 2048,
   typename TAllocator = std::allocator<TValue>>
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
      inline const TValue* GetPtr() const { return reinterpret_cast<TValue*>(m_storage); }
   };

   struct Chunk
   {
      IndexType m_nextFreeChunk = -1;
      IndexType m_firstFreeSlot = -1;
      IndexType m_lastFreeSlot = -1;

      std::bitset<ChunkSize> m_liveBits;
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
   ChunkedSlotMapStorage(const ChunkedSlotMapStorage&) = delete;
   ChunkedSlotMapStorage(ChunkedSlotMapStorage&& other);

   inline ~ChunkedSlotMapStorage() { Clear(); }

   inline SizeType Size() const { return m_size; }
   inline SizeType Capacity() const { return m_chunks.size() * ChunkSize; }

   TValue* GetPtr(TKey key) const;

   bool FindNextKey(TKey& key) const;
   TKey IncrementKey(TKey key) const;

   void AllocateChunk();
   KeyType AllocateSlot(ValueType*& outPtr);
   bool FreeSlot(KeyType key);
   void FreeSlotByIndex(IndexType chunkIndex, IndexType slotIndex);
 
   void Clear();

   using ChunkAllocator = typename std::allocator_traits<TAllocator>::template rebind_alloc<Chunk>;
   using ChunkPtrAllocator = typename std::allocator_traits<TAllocator>::template rebind_alloc<Chunk*>;

   SizeType m_size = 0;
   IndexType m_firstFreeChunk = -1;
   std::vector<Chunk*, ChunkPtrAllocator> m_chunks;
};


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
   SlotMap(const SlotMap& other) = delete;
   inline SlotMap(SlotMap&& other) : m_storage(std::move(other.m_storage)) {}

   SlotMap& operator=(const SlotMap& other) = delete;
   SlotMap& operator=(SlotMap&& other);

   inline SizeType Size() const { return m_storage.Size(); }
   inline SizeType Capacity() const { return m_storage.Capacity(); }
   bool Reserve(SizeType capacity);

   template<typename... TArgs>
   TKey Emplace(TArgs&&... args);

   inline bool Erase(TKey key) { return m_storage.FreeSlot(key); }
   inline void Clear() { m_storage.Clear(); }

   inline TValue* GetPtr(TKey key) { return m_storage.GetPtr(key); }
   inline const TValue* GetPtr(TKey key) const { return m_storage.GetPtr(key); }

   inline bool FindNextKey(TKey& key) const { return m_storage.FindNextKey(key); }
   inline TKey IncrementKey(TKey key) const { return m_storage.IncrementKey(key); }

private:
   TStorage m_storage;
};


template<typename TValue, size_t Capacity, typename TKey = uint32_t>
using FixedSlotMap = SlotMap<TValue, TKey, FixedSlotMapStorage<TValue, TKey, Capacity>>;


} // namespace slotmap


#include "slotmap.inl"

