#pragma once

#include "Buffer.h"
#include <deque>

namespace SpatialIndex
{
	namespace StorageManager
	{
		class FIFOEvictionsBuffer : public Buffer
		{
		public:
			FIFOEvictionsBuffer(IStorageManager&, Tools::PropertySet& ps);
				// see Buffer.h for available properties.

			~FIFOEvictionsBuffer() override;

			void addEntry(id_type page, Buffer::Entry* pEntry) override;
			void removeEntry() override;
			void loadByteArray(const id_type page, uint32_t& len, uint8_t** data) override;
			void storeByteArray(id_type& page, const uint32_t len, const uint8_t* const data) override;
			void deleteByteArray(const id_type page) override;

		private:
			std::deque<id_type> m_fifo_queue; // Queue to track insertion order
		}; // FIFOEvictionsBuffer
	}
}


