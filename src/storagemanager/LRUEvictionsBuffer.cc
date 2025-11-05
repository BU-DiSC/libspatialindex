#include <spatialindex/SpatialIndex.h>
#include <cstring>
#include "LRUEvictionsBuffer.h"

using namespace SpatialIndex;
using namespace SpatialIndex::StorageManager;

IBuffer* SpatialIndex::StorageManager::returnLRUEvictionsBuffer(IStorageManager& sm, Tools::PropertySet& ps)
{
	IBuffer* b = new LRUEvictionsBuffer(sm, ps);
	return b;
}

IBuffer* SpatialIndex::StorageManager::createNewLRUEvictionsBuffer(IStorageManager& sm, uint32_t capacity, bool bWriteThrough)
{
	Tools::Variant var;
	Tools::PropertySet ps;

	var.m_varType = Tools::VT_ULONG;
	var.m_val.ulVal = capacity;
	ps.setProperty("Capacity", var);

	var.m_varType = Tools::VT_BOOL;
	var.m_val.blVal = bWriteThrough;
	ps.setProperty("WriteThrough", var);

	return returnLRUEvictionsBuffer(sm, ps);
}

LRUEvictionsBuffer::LRUEvictionsBuffer(IStorageManager& sm, Tools::PropertySet& ps) : Buffer(sm, ps)
{
}

LRUEvictionsBuffer::~LRUEvictionsBuffer()
= default;

void LRUEvictionsBuffer::moveToBack(id_type page)
{
	auto map_it = m_lru_map.find(page);
	if (map_it != m_lru_map.end())
	{
		// Move the page to the back (most recently used)
		m_lru_list.splice(m_lru_list.end(), m_lru_list, map_it->second);
		// Update iterator (it's still valid, just pointing to moved element)
		map_it->second = --m_lru_list.end();
	}
}

void LRUEvictionsBuffer::addEntry(id_type page, Entry* e)
{
	assert(m_buffer.size() <= m_capacity);

	if (m_buffer.size() == m_capacity) removeEntry();
	assert(m_buffer.find(page) == m_buffer.end());
	m_buffer.insert(std::pair<id_type, Entry*>(page, e));
	
	// Add to back of LRU list (most recently used)
	m_lru_list.push_back(page);
	m_lru_map[page] = --m_lru_list.end();
}

void LRUEvictionsBuffer::removeEntry()
{
	if (m_buffer.size() == 0 || m_lru_list.empty()) return;

	// Remove the least recently used entry (front of list)
	id_type page = m_lru_list.front();
	m_lru_list.pop_front();
	m_lru_map.erase(page);

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

void LRUEvictionsBuffer::loadByteArray(const id_type page, uint32_t& len, uint8_t** data)
{
	auto it = m_buffer.find(page);

	if (it != m_buffer.end())
	{
		++m_u64Hits;
		len = (*it).second->m_length;
		*data = new uint8_t[len];
		memcpy(*data, (*it).second->m_pData, len);
		// Move to most recently used position
		moveToBack(page);
	}
	else
	{
		m_pStorageManager->loadByteArray(page, len, data);
		addEntry(page, new Entry(len, static_cast<const uint8_t*>(*data)));
	}
}

void LRUEvictionsBuffer::storeByteArray(id_type& page, const uint32_t len, const uint8_t* const data)
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
			// Move to most recently used position
			moveToBack(page);
		}
		else
		{
			addEntry(page, e);
		}
	}
}

void LRUEvictionsBuffer::deleteByteArray(const id_type page)
{
	auto it = m_buffer.find(page);
	if (it != m_buffer.end())
	{
		delete (*it).second;
		m_buffer.erase(it);
		
		// Remove from LRU tracking
		auto map_it = m_lru_map.find(page);
		if (map_it != m_lru_map.end())
		{
			m_lru_list.erase(map_it->second);
			m_lru_map.erase(map_it);
		}
	}

	m_pStorageManager->deleteByteArray(page);
}


