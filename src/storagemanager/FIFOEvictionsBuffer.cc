#include <spatialindex/SpatialIndex.h>
#include <cstring>
#include "FIFOEvictionsBuffer.h"

using namespace SpatialIndex;
using namespace SpatialIndex::StorageManager;

IBuffer* SpatialIndex::StorageManager::returnFIFOEvictionsBuffer(IStorageManager& sm, Tools::PropertySet& ps)
{
	IBuffer* b = new FIFOEvictionsBuffer(sm, ps);
	return b;
}

IBuffer* SpatialIndex::StorageManager::createNewFIFOEvictionsBuffer(IStorageManager& sm, uint32_t capacity, bool bWriteThrough)
{
	Tools::Variant var;
	Tools::PropertySet ps;

	var.m_varType = Tools::VT_ULONG;
	var.m_val.ulVal = capacity;
	ps.setProperty("Capacity", var);

	var.m_varType = Tools::VT_BOOL;
	var.m_val.blVal = bWriteThrough;
	ps.setProperty("WriteThrough", var);

	return returnFIFOEvictionsBuffer(sm, ps);
}

FIFOEvictionsBuffer::FIFOEvictionsBuffer(IStorageManager& sm, Tools::PropertySet& ps) : Buffer(sm, ps)
{
}

FIFOEvictionsBuffer::~FIFOEvictionsBuffer()
= default;

void FIFOEvictionsBuffer::addEntry(id_type page, Entry* e)
{
	assert(m_buffer.size() <= m_capacity);

	if (m_buffer.size() == m_capacity) removeEntry();
	assert(m_buffer.find(page) == m_buffer.end());
	m_buffer.insert(std::pair<id_type, Entry*>(page, e));
	m_fifo_queue.push_back(page);
}

void FIFOEvictionsBuffer::removeEntry()
{
	if (m_buffer.size() == 0 || m_fifo_queue.empty()) return;

	// Remove the oldest entry (front of queue)
	id_type page = m_fifo_queue.front();
	m_fifo_queue.pop_front();

	auto it = m_buffer.find(page);
	if (it != m_buffer.end())
	{
		if ((*it).second->m_bDirty)
		{
			m_pStorageManager->storeByteArray(page, ((*it).second)->m_length, (const uint8_t*) ((*it).second)->m_pData);
		}

		delete (*it).second;
		m_buffer.erase(it);
	}
}

void FIFOEvictionsBuffer::loadByteArray(const id_type page, uint32_t& len, uint8_t** data)
{
	auto it = m_buffer.find(page);

	if (it != m_buffer.end())
	{
		++m_u64Hits;
		len = (*it).second->m_length;
		*data = new uint8_t[len];
		memcpy(*data, (*it).second->m_pData, len);
		// For FIFO, we don't change the order - no need to move to back
	}
	else
	{
		m_pStorageManager->loadByteArray(page, len, data);
		addEntry(page, new Entry(len, static_cast<const uint8_t*>(*data)));
	}
}

void FIFOEvictionsBuffer::storeByteArray(id_type& page, const uint32_t len, const uint8_t* const data)
{
	if (page == NewPage)
	{
		m_pStorageManager->storeByteArray(page, len, data);
		assert(m_buffer.find(page) == m_buffer.end());
		addEntry(page, new Entry(len, data));
	}
	else
	{
		if (m_bWriteThrough)
		{
			m_pStorageManager->storeByteArray(page, len, data);
		}

		Entry* e = new Entry(len, data);
		if (m_bWriteThrough == false) e->m_bDirty = true;

		auto it = m_buffer.find(page);
		if (it != m_buffer.end())
		{
			delete (*it).second;
			(*it).second = e;
			if (m_bWriteThrough == false) ++m_u64Hits;
			// For FIFO, we don't change the order when updating existing entry
		}
		else
		{
			addEntry(page, e);
		}
	}
}

void FIFOEvictionsBuffer::deleteByteArray(const id_type page)
{
	auto it = m_buffer.find(page);
	if (it != m_buffer.end())
	{
		delete (*it).second;
		m_buffer.erase(it);
		// Remove from FIFO queue
		for (auto qit = m_fifo_queue.begin(); qit != m_fifo_queue.end(); ++qit)
		{
			if (*qit == page)
			{
				m_fifo_queue.erase(qit);
				break;
			}
		}
	}

	m_pStorageManager->deleteByteArray(page);
}

