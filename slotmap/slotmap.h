// vim: et:ts=3:sw=3:sts=3
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
#include <limits>
#include <vector>
#include <bitset>
#include <memory>
#include <cassert>

#include "bitset.h"


/**
 * Slotmap implementation.
 */
namespace slotmap {


namespace impl {


//////////////////////////////////////////////////////////////////////////
/**
 * Returns the number of bits required to represent the given number of
 * elements.
 *
 * \relates FixedSlotMapStorage
 * \relates ChunkedSlotMapStorage
 */
constexpr int GetIndexBitSize(uintmax_t arraySize)
{
   uintmax_t maxIndex = 1;

   for (int bitCount = 1; bitCount < std::numeric_limits<uintmax_t>::digits; ++bitCount)
   {
      if ((maxIndex + 1) >= arraySize)
      {
         return bitCount;
      }
      maxIndex = (maxIndex << 1) | static_cast<uintmax_t>(1);
   }

   return std::numeric_limits<uintmax_t>::digits;
}


} // namespace impl


//////////////////////////////////////////////////////////////////////////
/**
 * Fixed-capacity statically allocated SlotMap storage.
 */
template<
   typename TValue,
   typename TKey,
   size_t TCapacity = 1024,
   typename TBitsetTraits = FixedBitSetTraits<>>
class FixedSlotMapStorage
{
public:
   using ValueType = TValue;
   using KeyType = TKey;

   using ReferenceType = ValueType&;
   using ConstReferenceType = const ValueType&;

   using SizeType = size_t;
   using IndexType = ptrdiff_t;

   using BitsetType = typename TBitsetTraits::template BitsetType<TCapacity>;

   static constexpr size_t  StaticCapacity = TCapacity;
   static constexpr KeyType InvalidKey = static_cast<KeyType>(0);

   static constexpr int KeyBitSize = static_cast<int>(sizeof(KeyType) * CHAR_BIT);
   static constexpr int SlotIndexBitSize = impl::GetIndexBitSize(StaticCapacity + 1);
   static_assert(SlotIndexBitSize > 0, "SlotIndexBitSize must be greater than 0.");
   static_assert(SlotIndexBitSize < KeyBitSize, "SlotIndexBitSize must be greater than 0.");

   static constexpr int GenerationBitSize = KeyBitSize - SlotIndexBitSize;
   static_assert(GenerationBitSize > 0, "GenerationBitSize must be greater than 0.");
   static_assert(GenerationBitSize <= 64, "GenerationBitSize must be less or equal to 64.");
   
   static constexpr KeyType SlotIndexMask = (static_cast<KeyType>(1) << SlotIndexBitSize) - 1;
   static_assert(SlotIndexMask >= StaticCapacity, "SlotIndexMask must be greater than StaticCapacity.");

   static constexpr KeyType GenerationShift = SlotIndexBitSize;
   static constexpr KeyType GenerationMask = (static_cast<KeyType>(1) << GenerationBitSize) - 1;

   using GenerationType = std::conditional_t<GenerationBitSize <= 8, uint8_t,
      std::conditional_t<GenerationBitSize <= 16, uint16_t,
      std::conditional_t<GenerationBitSize <= 32, uint32_t,
      uint64_t>>>;

   static_assert(std::is_unsigned_v<KeyType>);
   static_assert(sizeof(GenerationType) <= sizeof(KeyType));

private:
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

public:
   template<bool IsConst>
   class IteratorTpl
   {
      friend class FixedSlotMapStorage;

   public:
      using StoragePtr = std::conditional_t<IsConst, const FixedSlotMapStorage*, FixedSlotMapStorage*>;
      using ReferenceType = std::conditional_t<IsConst, const ValueType&, ValueType&>;
      using PointerType = std::conditional_t<IsConst, const ValueType*, ValueType*>;

      IteratorTpl() = default;

   private:
      constexpr IteratorTpl(StoragePtr storage, KeyType key) : m_storage(storage), m_key(key) {}
      constexpr IteratorTpl(StoragePtr storage) : m_storage(storage) {}
   
   public:
      inline bool operator==(const IteratorTpl& other) const { return m_key == other.m_key; }
      inline bool operator!=(const IteratorTpl& other) const { return m_key != other.m_key; }

      inline IteratorTpl& operator++() { Advance(); return *this; }
      inline IteratorTpl operator++(int) { IteratorTpl it(*this); Advance(); return it; }

      inline KeyType GetKey() const { return m_key; }
      inline PointerType GetPtr() const { return m_ptr; }
      
      bool Advance();
   
   private:
      bool FindNext();

      StoragePtr m_storage = nullptr;
      KeyType m_key = std::numeric_limits<KeyType>::max();
      PointerType m_ptr = nullptr;
   };
   
   using Iterator = IteratorTpl<false>;
   using ConstIterator = IteratorTpl<true>;

   FixedSlotMapStorage();
   FixedSlotMapStorage(const FixedSlotMapStorage&);
   FixedSlotMapStorage(FixedSlotMapStorage&& other);

   ~FixedSlotMapStorage();

   FixedSlotMapStorage& operator=(const FixedSlotMapStorage&) = delete;
   FixedSlotMapStorage& operator=(FixedSlotMapStorage&&);

   inline SizeType Size() const { return m_size; }
   inline constexpr SizeType Capacity() const { return StaticCapacity; }
   inline static constexpr SizeType MaxCapacity() { return StaticCapacity; }

   bool Reserve(size_t capacity);

   template<typename TSelf>
   static inline auto GetPtrTpl(TSelf self, TKey key);

   inline TValue* GetPtr(TKey key) { return GetPtrTpl(this, key); }
   inline const TValue* GetPtr(TKey key) const { return GetPtrTpl(this, key); }
   
   TKey GetKeyByIndex(SizeType index) const;
   SizeType GetIndexByKey(TKey key) const;
   
   bool FindNextKey(TKey& key) const;
   TKey IncrementKey(TKey key) const;

   template<typename TFunc>
   void ForEachSlot(TFunc func) const;
   
   KeyType ReserveSlot(ValueType*& outPtr);
   inline KeyType ReserveSlotNoAlloc(ValueType*& outPtr) { return ReserveSlot(outPtr); }
   bool FreeSlot(KeyType key);

   void Swap(FixedSlotMapStorage& other);
   void Clear();
   
   Iterator Begin() { Iterator it(this, 0); it.FindNext(); return it; }
   constexpr Iterator End() { return Iterator(this); }
   
   ConstIterator Begin() const { ConstIterator it(this, 0); it.FindNext(); return it; }
   constexpr ConstIterator End() const { return ConstIterator(this); }

private:
   SizeType m_size = 0;
   IndexType m_firstFreeSlot = -1;
   IndexType m_maxUsedSlot = 0;
   BitsetType m_liveBits;
   GenerationType m_generations[TCapacity];
   Slot m_slots[TCapacity];
};


//////////////////////////////////////////////////////////////////////////
constexpr size_t DefaultMaxChunkSize = 4096;
constexpr size_t MinChunkSlots = 4;


//////////////////////////////////////////////////////////////////////////
template<size_t TSlotCount, typename TValue, typename TIndexType, typename TGenerationType, typename TBitsetTraits>
struct ChunkTpl
{
   struct Slot
   {
      union
      {
         alignas(TValue) uint8_t m_storage[sizeof(TValue)];
         TIndexType m_nextFreeSlot;
      };
   
      inline TValue* GetPtr() { return reinterpret_cast<TValue*>(m_storage); }
      inline const TValue* GetPtr() const { return reinterpret_cast<const TValue*>(m_storage); }
   };

   using BitsetType = typename TBitsetTraits::template BitsetType<TSlotCount>;

   ChunkTpl() = default;
   ChunkTpl(const ChunkTpl& other);

   TIndexType m_nextFreeChunk = -1;
   TIndexType m_firstFreeSlot = -1;
   TIndexType m_lastFreeSlot = -1;

   BitsetType m_liveBits;
   TGenerationType m_generations[TSlotCount];
   Slot m_slots[TSlotCount];
};


//////////////////////////////////////////////////////////////////////////
/**
 * Returns the maximal number of slots that can be stored in a chunk such that
 * the size of the chunk is less or equal to the given size.
 */
namespace impl {
template<
   size_t MinSlots,
   size_t MaxSlots,
   size_t MaxChunkSize,
   typename TValueType,
   typename TIndexType,
   typename TGenerationType,
   typename TBitsetTraits>
constexpr size_t GetChunkMaxSlots()
{
   if constexpr (MaxSlots <= MinSlots)
   {
      return MinSlots;
   }
   else if constexpr (sizeof(ChunkTpl<MinSlots, TValueType, TIndexType, TGenerationType, TBitsetTraits>) >= MaxChunkSize)
   {
      return MinSlots;
   }
   else if constexpr (sizeof(ChunkTpl<MaxSlots, TValueType, TIndexType, TGenerationType, TBitsetTraits>) <= MaxChunkSize)
   {
      return MaxSlots;
   }
   else
   {
      constexpr size_t pivot = (MinSlots + MaxSlots) >> 1;

      if constexpr (pivot == MinSlots)
      {
         return MinSlots;
      }
      else if constexpr (sizeof(ChunkTpl<pivot, TValueType, TIndexType, TGenerationType, TBitsetTraits>) > MaxChunkSize)
      {
         return GetChunkMaxSlots<MinSlots, pivot - 1, MaxChunkSize, TValueType, TIndexType, TGenerationType, TBitsetTraits>();
      }
      else
      {
         return GetChunkMaxSlots<pivot, MaxSlots, MaxChunkSize, TValueType, TIndexType, TGenerationType, TBitsetTraits>();
      }
   }

   return MinSlots;
}
} // namespace impl


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
class ChunkedSlotMapStorage
{
public:
   using ValueType = TValue;
   using KeyType = TKey;
   using GenerationType = uint8_t;

   using SizeType = size_t;
   using IndexType = ptrdiff_t;

   static_assert(std::is_unsigned_v<KeyType>, "Slotmap key type must be an unsigned integer type.");
   static_assert(sizeof(KeyType) > sizeof(GenerationType), "The size of slotmap key type must be greater than the size of generation type.");

   static constexpr KeyType InvalidKey = static_cast<KeyType>(0);
   
   static constexpr size_t MaxChunkSlots = impl::GetChunkMaxSlots<MinChunkSlots, MaxChunkSize, MaxChunkSize, ValueType, IndexType, GenerationType, TBitsetTraits>();
   static constexpr int GenerationBitSize = sizeof(GenerationType) * CHAR_BIT;
   static constexpr int SlotIndexBitSize = std::min(
      impl::GetIndexBitSize(MaxChunkSlots), 
      static_cast<int>(sizeof(KeyType) * CHAR_BIT - GenerationBitSize - 1));
   static constexpr int ChunkIndexBitSize = (sizeof(TKey) * CHAR_BIT) - GenerationBitSize - SlotIndexBitSize;
   
   static constexpr KeyType ChunkSlots = std::min<KeyType>(static_cast<KeyType>(MaxChunkSlots), static_cast<KeyType>(1) << SlotIndexBitSize);
   static_assert(ChunkSlots > 0, "Chunk must contain more than 0 slots.");
   
   static constexpr KeyType ChunkIndexMask = (static_cast<KeyType>(1) << ChunkIndexBitSize) - 1;
   static constexpr KeyType MaxChunkCount = ChunkIndexMask;
   
   static constexpr KeyType SlotIndexShift = ChunkIndexBitSize;
   static constexpr KeyType SlotIndexMask = (static_cast<KeyType>(1) << SlotIndexBitSize) - 1;
   
   static constexpr KeyType GenerationShift = ChunkIndexBitSize + SlotIndexBitSize;
   static constexpr KeyType GenerationMask = (static_cast<KeyType>(1) << GenerationBitSize) - 1;

   using BitsetType = typename TBitsetTraits::template BitsetType<ChunkSlots>;

   static_assert(SlotIndexBitSize > 0);
   static_assert(ChunkIndexBitSize > 0);
   
   using Chunk = ChunkTpl<ChunkSlots, ValueType, IndexType, GenerationType, TBitsetTraits>;
   using Slot = typename Chunk::Slot;
   static_assert(sizeof(Chunk) <= MaxChunkSize, "Chunk size is too large.");

   template<bool IsConst>
   class IteratorTpl
   {
      friend class ChunkedSlotMapStorage;

   public:
      using StoragePtr = std::conditional_t<IsConst, const ChunkedSlotMapStorage*, ChunkedSlotMapStorage*>;
      using ReferenceType = std::conditional_t<IsConst, const ValueType&, ValueType&>;
      using PointerType = std::conditional_t<IsConst, const ValueType*, ValueType*>;

      IteratorTpl() = default;

   private:
      constexpr IteratorTpl(StoragePtr storage, size_t chunkIndex, size_t slotIndex) 
         : m_storage(storage), m_chunkIndex(chunkIndex), m_slotIndex(slotIndex) 
      {}
      constexpr IteratorTpl(StoragePtr storage) 
         : m_storage(storage) 
      {}

   public:
      inline bool operator==(const IteratorTpl& other) const { return m_key == other.m_key; }
      inline bool operator!=(const IteratorTpl& other) const { return m_key != other.m_key; }

      inline IteratorTpl& operator++() { Advance(); return *this; }
      inline IteratorTpl operator++(int) { const IteratorTpl it(*this); Advance(); return it; }
      
      inline KeyType GetKey() const { return m_key; }
      inline PointerType GetPtr() const { return m_ptr; }
      
      /**
       * Moves the iterator to the next valid element if there is one or to the end otherwise.
       *
       * \return `true` if after the call the iterator points to a valid element, `false` otherwise.
       */
      bool Advance();
   
   private:
      void FindFirst();

      bool FindNext();

      StoragePtr const m_storage = nullptr;
      SizeType m_chunkIndex = 0;
      SizeType m_slotIndex = 0;

      KeyType m_key = std::numeric_limits<KeyType>::max();
      PointerType m_ptr = nullptr;
   };

   using Iterator = IteratorTpl<false>;
   using ConstIterator = IteratorTpl<true>;
   
   ChunkedSlotMapStorage() = default;
   ChunkedSlotMapStorage(const ChunkedSlotMapStorage&);
   ChunkedSlotMapStorage(ChunkedSlotMapStorage&& other);

   inline ~ChunkedSlotMapStorage() { Clear(); }

   ChunkedSlotMapStorage& operator=(const ChunkedSlotMapStorage&) = delete;
   ChunkedSlotMapStorage& operator=(ChunkedSlotMapStorage&& other);

   inline SizeType Size() const { return m_size; }
   inline SizeType Capacity() const { return m_chunks.size() * ChunkSlots; }
   inline static constexpr SizeType MaxCapacity() { return MaxChunkCount * ChunkSlots; }
   
   bool Reserve(size_t capacity);
   
   TValue* GetPtr(TKey key) const;

   SizeType GetIndexByKey(KeyType key) const;
   KeyType GetKeyByIndex(SizeType index) const;

   bool FindNextKey(TKey& key) const;
   KeyType IncrementKey(TKey key) const;

   template<typename TFunc>
   void ForEachSlot(TFunc func) const;

   void AllocateChunk();
   static void InitializeChunk(Chunk* chunk);
   void AppendChunkToFreeList(Chunk* chunk, IndexType chunkIndex);
   KeyType ReserveSlot(ValueType*& outPtr);
   KeyType ReserveSlotNoAlloc(ValueType*& outPtr);
   bool FreeSlot(KeyType key);
   void FreeSlotByIndex(IndexType chunkIndex, IndexType slotIndex);
   
   void Swap(ChunkedSlotMapStorage& other);
   void Clear();
   
   Iterator Begin() { Iterator it(this, 0, 0); it.FindFirst(); return it; }
   constexpr Iterator End() { return Iterator(this); }
   
   ConstIterator Begin() const { ConstIterator it(this, 0, 0); it.FindFirst(); return it; }
   constexpr ConstIterator End() const { return ConstIterator(this); }

private:
   using ChunkAllocator = typename std::allocator_traits<TAllocator>::template rebind_alloc<Chunk>;
   using ChunkPtrAllocator = typename std::allocator_traits<TAllocator>::template rebind_alloc<Chunk*>;

   SizeType m_size = 0;
   IndexType m_firstFreeChunk = -1;
   SizeType m_maxUsedChunk = 0;
   std::vector<Chunk*, ChunkPtrAllocator> m_chunks;
};


//////////////////////////////////////////////////////////////////////////
/**
 * Associative container with *O(1)* insertion, removal and lookup times.
 */
template<typename TValue, typename TKey = uint32_t, typename TStorage = ChunkedSlotMapStorage<TValue, TKey>>
class SlotMap
{
public:
   using ValueType = typename TStorage::ValueType;
   using KeyType = typename TStorage::KeyType;
   using SizeType = typename TStorage::SizeType;
   using Iterator = typename TStorage::Iterator;
   using ConstIterator = typename TStorage::ConstIterator;

   /**
    * A key that is always guaranteed to be invalid.
    */
   static constexpr KeyType InvalidKey = TStorage::InvalidKey;

   /**
    * Default constructor.
    * 
    * Constructs empty slot map.
    */
   SlotMap() = default;
   /**
    * Copy constructor.
    * 
    * Copies all elements from `other` to the slotmap. The copied elements will
    * have the same keys.
    */
   inline SlotMap(const SlotMap& other) : m_storage(other.m_storage) {}
   /**
    * Move constructor.
    *
    * Constructs the slotmap by moving the contents of `other` to the new slotmap.
    */
   inline SlotMap(SlotMap&& other) : m_storage(std::move(other.m_storage)) {}
   
   /**
    * Copy assignment operator.
    * 
    * The copied elements will have the same keys.
    */
   inline SlotMap& operator=(const SlotMap& other) { m_storage = other.m_storage; return *this; }
   /**
    * Move assignment operator.
    */
   inline SlotMap& operator=(SlotMap&& other) { m_storage = std::move(other.m_storage); return *this; }
   
   /**
    * \name Capacity
    */
   ///@{
   /**
    * Returns the number of elements that the slotmap holds.
    */
   inline SizeType Size() const { return m_storage.Size(); }
   /**
    * Returns the number of elements that can be held in the currently allocated storage.
    */
   inline SizeType Capacity() const { return m_storage.Capacity(); }
   /**
    * Returns the number of elements that can potentially be held in the slotmap.
    * 
    * The maximum capacity depends on the key type, the storage
    * type and the value type.
    */
   inline static constexpr SizeType MaxCapacity() { return TStorage::MaxCapacity(); }
   /**
    * If possible, increases the capacity of the slotmap to at least `capacity`
    * and returns `true`, otherwise returns `false`.
    *
    * If the function returns `false`, the capacity remains unchanged.
    *
    * In either case, all existing keys remain valid. The \ref MaxCapacity()
    * function can be used to determine the maximum capacity that can be
    * reserved in a slotmap.
    *
    * \param capacity New capacity of the slotmap, in number of elements.
    */
   inline bool Reserve(SizeType capacity) { return m_storage.Reserve(capacity); }
   ///@}

   /**
    * \name Modifiers
    */
   ///@{
   /**
    * Constructs a new element in a free slot and returns the key associated with it.
    *
    * Has O(1) time complexity.
    *
    * \param args The arguments to be forwarded to the constructor of the element.
    * \return They key associated with the newly constructed element.
    */
   template<typename... TArgs>
   TKey Emplace(TArgs&&... args);
   /**
    * Constructs a new element in a free slot if one can be reserved without doing
    * any allocation and returns the key associated with it, otherwise returns
    * \ref InvalidKey.
    *
    * Has O(1) time complexity in the word case.
    *
    * \param args The arguments to be forwarded to the constructor of the element.
    * \return They key associated with the newly constructed element.
    */
   template<typename... TArgs>
   TKey EmplaceNoAlloc(TArgs&&... args);

   /**
    * Erases the element with the given key.
    *
    * If the specified key is valid, erases the associated element and returns
    * `true`. Otherwise, returns `false`.
    *
    * After this call, \ref GetPtr() returns nullptr for the given key.
    *
    * Has O(1) time complexity (worst case).
    *
    * \param key The key of the element to be erased.
    */
   inline bool Erase(TKey key) { return m_storage.FreeSlot(key); }
   /**
    * Exchanges the contents of the slotmap with those of `other`.
    *
    * Depending on the storage implementation, this operation may or may not
    * move elements (the \ref FixedSlotMapStorage moves elements, the 
    * \ref ChunkedSlotMapStorage does not).
    *
    * The time complexity depends on the storage implementation.
    *
    * \param other The slotmap to swap with.
    */
   inline void Swap(SlotMap& other) { m_storage.Swap(other.m_storage); }
   /**
    * Erases all elements from the slotmap.
    *
    * After this call, \ref Size() returns zero. Invalidates all keys.
    *
    * Leaves the capacity unchanged. No memory is deallocated.
    *
    * Has O(n) time complexity where n is the max. number of elements that the
    * slotmap contained since construction or last \ref Clear() call (i.e. not
    * the capacity).
    */
   inline void Clear() { m_storage.Clear(); }
   ///@}

   /**
    * \name Element access
    */
   ///@{
   /**
    * Returns a pointer to the element associated with the given key.
    *
    * If the key is invalid, returns `nullptr`.
    *
    * Has O(1) time complexity (worst case).
    *
    * \param key The key of the element to be retrieved.
    * \return A pointer to the element associated with the given key or `nullptr` is the key is invalid.
    */
   inline TValue* GetPtr(TKey key) { return m_storage.GetPtr(key); }
   /**
    * Returns a pointer to the element associated with the given key.
    *
    * If the key is invalid, returns `nullptr`.
    *
    * Has O(1) time complexity (worst case).
    *
    * \param key The key of the element to be retrieved.
    * \return A pointer to the element associated with the given key or `nullptr` is the key is invalid.
    */
   inline const TValue* GetPtr(TKey key) const { return m_storage.GetPtr(key); }
   
   /**
    * Returns the key associated with the element at the given index.
    */
   inline TKey GetKeyByIndex(SizeType index) const { return m_storage.GetKeyByIndex(index); }
   /**
    * @brief Returns a pointer to the element at the given index.
    * @param index Index of the element.
    * @return Pointer to the element at the given index.
    */
   TValue* GetPtrByIndex(SizeType index) { return m_storage.GetPtr(GetKeyByIndex(index)); }
   /**
    * @brief Returns a pointer to the element at the given index.
    * @param index Index of the element.
    * @return Pointer to the element at the given index.
    */
   const TValue* GetPtrByIndex(SizeType index) const { return m_storage.GetPtr(GetKeyByIndex(index)); }
   /**
    * @brief Returns the index of the element with the given key.
    * @param key Key of the element.
    * @return Index of the element with the given key.
    */
   SizeType GetIndexByKey(TKey key) const { return m_storage.GetIndexByKey(key); }
   ///@}

   /**
    * \name Iterators
    */
   ///@{

   /**
    * Starting from the given key (which may not be valid), find the next valid
    * key and return it.
    *
    * This function is meant to be used together with \ref IncrementKey() to
    * iterate over all valid keys. Example:
    *
    * \code
    * SlotMap<int> slotmap;
    * for (SlotMap<int>::KeyType key = 0; slotmap.FindNextKey(key); key = slotmap.IncrementKey(key))
    * {
    *   // Do something with the key
    * }
    * \endcode
    *
    * \note If performance is critical, consider using \ref ForEach() instead.
    */
   inline bool FindNextKey(TKey& key) const { return m_storage.FindNextKey(key); }
   /**
    * Increments the key to the next slot, valid or not.
    *
    * This function is meant to be used together with \ref FindNextKey() to
    * iterate over all valid keys.
    */
   inline TKey IncrementKey(TKey key) const { return m_storage.IncrementKey(key); }

   /**
    * Applies the given function object to each valid element in the slotmap.
    *
    * \param func The function object to be applied every element. The signature
    *             of the function should be equivalent to `void func(TKey key, TValue& value)`.
    */
   template<typename TFunc>
   inline void ForEach(TFunc func) const { m_storage.ForEachSlot(func); }
   
   /**
    * Returns an iterator to the first element in the slotmap if it's not empty,
    * othersie returns \ref End().
    * 
    * Has *O(n)* time complexity in the worst case.
    */
   inline Iterator Begin() { return m_storage.Begin(); }
   /**
    * Returns an iterator to the first element in the slotmap if it's not empty,
    * othersie returns \ref End().
    * 
    * Has *O(n)* time complexity in the worst case.
    */
   inline ConstIterator Begin() const { return m_storage.Begin(); }

   /**
    * Returns an iterator to the element following the last element of the slotmap.
    *
    * Has *O(1)* time complexity.
    */
   constexpr inline Iterator End() { return m_storage.End(); }
   /**
    * Returns an iterator to the element following the last element of the slotmap.
    *
    * Has *O(1)* time complexity.
    */
   constexpr inline ConstIterator End() const { return m_storage.End(); }
   
   ///@}

private:
   TStorage m_storage;
};


/**
 * \ref SlotMap implementation with fixed capacity statically allocated storage.
 */
template<typename TValue, size_t Capacity, typename TKey = uint32_t>
using FixedSlotMap = SlotMap<TValue, TKey, FixedSlotMapStorage<TValue, TKey, Capacity>>;


} // namespace slotmap


#include "slotmap.inl"

