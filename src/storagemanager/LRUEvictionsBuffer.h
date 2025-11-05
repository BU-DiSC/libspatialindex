#pragma once

#include "Buffer.h"
#include <list>
#include <unordered_map>

namespace SpatialIndex
{
	namespace StorageManager
	{
		class LRUEvictionsBuffer : public Buffer
		{
		public:
			LRUEvictionsBuffer(IStorageManager&, Tools::PropertySet& ps);
			~LRUEvictionsBuffer() override;

			void addEntry(id_type page, Buffer::Entry* pEntry) override;
			void removeEntry() override;
			void loadByteArray(const id_type page, uint32_t& len, uint8_t** data) override;
			void storeByteArray(id_type& page, const uint32_t len, const uint8_t* const data) override;
			void deleteByteArray(const id_type page) override;

		private:
			// LRU list: front is least recently used, back is most recently used
			std::list<id_type> m_lru_list;
			// Map from page ID to iterator in LRU list for O(1) access
			std::unordered_map<id_type, std::list<id_type>::iterator> m_lru_map;

			void moveToBack(id_type page); // Move page to most recently used position
		}; // LRUEvictionsBuffer
	}
}


