/* Summer 2017 */
#include "coherenceUtils.h"
#include "coherenceWrite.h"
#include "../cache/mem.h"
#include "../cache/getFromCache.h"
#include "../cache/setInCache.h"
#include "../cache/cacheWrite.h"
#include "../cache/cacheRead.h"
#include "../hitrate/hitRate.h"

/*
	A function which processes all cache writes for an entire cache system.
	Takes in a cache system, an address, an id for a cache, an address, a size
	of data, and a pointer to data and calls the appropriate functions on the 
	cache being selected to write to the cache. 
*/
void cacheSystemWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint8_t size, uint8_t* data) {
	//uint8_t* transferData;
	evictionInfo_t* dstCacheInfo;
	evictionInfo_t* otherCacheInfo;
	uint32_t evictionBlockNumber;
	//uint32_t offset;
	cacheNode_t** caches;
	uint32_t tagVal;
	//int otherCacheContains = 0;
	cache_t* dstCache = NULL;
	uint8_t counter = 0;
	caches = cacheSystem->caches;
	while (dstCache == NULL && counter < cacheSystem->size) { //Selects destination cache pointer from array of caches pointers
		if (caches[counter]->ID == ID) {
			dstCache = caches[counter]->cache;
		}
		counter++;
	}
	tagVal = getTag(dstCache, address);
	dstCacheInfo = findEviction(dstCache, address); //Finds block to evict and potential match
	evictionBlockNumber = dstCacheInfo->blockNumber;
	if (dstCacheInfo->match) {
		/*What do you do if it is in the cache?*/
		/*Your Code Here*/

		writeToCache(dstCache, address, data, size);
		setState(dstCache, evictionBlockNumber, MODIFIED);
		cache_t* temp;
		for (int i = 0; i < cacheSystem->size; i++) {
			if (caches[i]->ID != ID && snooperContains(cacheSystem->snooper, address, caches[i]->ID)) {
				temp = caches[i]->cache;
				removeFromSnooper(cacheSystem->snooper, address, caches[i]->ID, cacheSystem->blockDataSize);
				updateState(temp, address, MODIFIED);
			}
		}

	} else {
		uint32_t oldAddress = extractAddress(dstCache, extractTag(dstCache, evictionBlockNumber), evictionBlockNumber, 0);
		/*How do you need to update the snooper?*/
		/*How do you need to update states for what is getting evicted (don't worry about evicting this will be handled at a later step when you place data in the cache)?*/
		/*Your Code Here*/
		removeFromSnooper(cacheSystem->snooper, oldAddress, ID, dstCache->blockDataSize);
		evict(dstCache, dstCacheInfo->blockNumber); //
		setState(dstCache, dstCacheInfo->blockNumber, INVALID);
		uint32_t oldAddrTag = getTag(dstCache, oldAddress);
		uint32_t oldAddrIndex = getIndex(dstCache, oldAddress);
		long oldLRU = dstCacheInfo->LRU;
		decrementLRU(dstCache, oldAddrTag, oldAddrIndex, oldLRU);

		cache_t* temp;
		for (int i = 0; i < cacheSystem->size; i++) {
			if (caches[i]->ID != ID && snooperContains(cacheSystem->snooper, oldAddress, caches[i]->ID)) {
				temp = caches[i]->cache;
				if (determineState(temp, oldAddress) == SHARED && returnIDIf1(cacheSystem->snooper, oldAddress, cacheSystem->blockDataSize)) {
					continue;
				}
				updateState(temp, oldAddress, INVALID);
			}
		}

		int val = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
		/*Check other caches???*/
		while(val != -1) {
			cache_t* other;
			for (int i = 0; i < cacheSystem->size; i++) {
				if (caches[i]->ID == val) {
					other = caches[i]->cache;
					break;
				}
			}
			otherCacheInfo = findEviction(other, address);
			enum state otherState = determineState(other, address);
			if (otherState == MODIFIED) {
				uint32_t addr = extractAddress(other, getTag(other, address), otherCacheInfo->blockNumber, 0);
				uint8_t* retVal = readFromCache(other, addr, other->blockDataSize);
				writeToCache(dstCache, addr, retVal, other->blockDataSize);
				free(retVal);
				free(otherCacheInfo);
				break;
			} else {
				removeFromSnooper(cacheSystem->snooper, address, val, cacheSystem->blockDataSize);
				setState(other, otherCacheInfo->blockNumber, INVALID);
				uint32_t addrTag = getTag(other, address);
				uint32_t addrIndex = getIndex(other, address);
				long oldLRU = otherCacheInfo->LRU;
				decrementLRU(other, addrTag, addrIndex, oldLRU);
				free(otherCacheInfo);
			}
			val = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
		}
		writeToCache(dstCache, address, data, size);
		setState(dstCache, evictionBlockNumber, MODIFIED);
		for (int i = 0; i < cacheSystem->size; i++) {
			if (caches[i]->ID != ID) {
				temp = caches[i]->cache;
				removeFromSnooper(cacheSystem->snooper, address, caches[i]->ID, cacheSystem->blockDataSize);
				updateState(temp, address, MODIFIED);
			}
		}
	}
	addToSnooper(cacheSystem->snooper, address, ID, cacheSystem->blockDataSize);
	free(dstCacheInfo);
}


/*
	A function used to write a byte to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemByteWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint8_t data) {
	/* Error Checking??*/
	if (!cacheSystem) {
		return -1;
	}
	bool realID = false;
	cacheNode_t** caches = cacheSystem->caches;
	for (int i = 0; i < cacheSystem->size; i++) {
		if (caches[i]->ID == ID) {
			realID = true;
			break;
		}
	}
	if (!realID) {
		return -1;
	}

	if (validAddresses(address, 8) == 0) {
		return -1;
	}

	uint8_t array[1];
	array[0] = data;
	cacheSystemWrite(cacheSystem, address, ID, 1, array);
	return 0;
}

/*
	A function used to write a halfword to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemHalfWordWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint16_t data) {
	/* Error Checking??*/
	if (!cacheSystem) {
		return -1;
	}
	bool realID = false;
	cacheNode_t** caches = cacheSystem->caches;
	for (int i = 0; i < cacheSystem->size; i++) {
		if (caches[i]->ID == ID) {
			realID = true;
			break;
		}
	}
	if (!realID) {
		return -1;
	}

	if (validAddresses(address, 8) == 0) {
		return -1;
	}

	if (cacheSystem->blockDataSize < 2) {
		cacheSystemByteWrite(cacheSystem, address, ID, (uint8_t) (data >> 8));
		cacheSystemByteWrite(cacheSystem, address + 1, ID, (uint8_t) (data & UINT8_MAX));
	}
	uint8_t array[2];
	array[0] = (uint8_t) (data >> 8);
	array[1] = (uint8_t) (data & UINT8_MAX);
	cacheSystemWrite(cacheSystem, address, ID, 2, array);
	return 0;
}

/*
	A function used to write a word to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemWordWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint32_t data) {
	/* Error Checking??*/
	if (!cacheSystem) {
		return -1;
	}
	bool realID = false;
	cacheNode_t** caches = cacheSystem->caches;
	for (int i = 0; i < cacheSystem->size; i++) {
		if (caches[i]->ID == ID) {
			realID = true;
			break;
		}
	}
	if (!realID) {
		return -1;
	}

	if (validAddresses(address, 8) == 0) {
		return -1;
	}

	if (cacheSystem->blockDataSize < 4) {
		cacheSystemByteWrite(cacheSystem, address, ID, (uint8_t) (data >> 16));
		cacheSystemByteWrite(cacheSystem, address + 2, ID, (uint8_t) (data & UINT16_MAX));
	}
	uint8_t array[4];
	array[0] = (uint8_t) (data >> 24);
	array[1] = (uint8_t) ((data >> 16) & UINT8_MAX);
	array[2] = (uint8_t) ((data >> 8) & UINT8_MAX);
	array[3] = (uint8_t) (data & UINT8_MAX);
	cacheSystemWrite(cacheSystem, address, ID, 4, array);
	return 0;
}

/*
	A function used to write a doubleword to a specific cache in a cache system.
	Takes in a cache system, an address, an ID, and data for the cache which
	will be written to. Returns 0 if the write is successful and otherwise
	returns -1.
*/
int cacheSystemDoubleWordWrite(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint64_t data) {
	/* Error Checking??*/
	if (!cacheSystem) {
		return -1;
	}
	bool realID = false;
	cacheNode_t** caches = cacheSystem->caches;
	for (int i = 0; i < cacheSystem->size; i++) {
		if (caches[i]->ID == ID) {
			realID = true;
			break;
		}
	}
	if (!realID) {
		return -1;
	}

	if (validAddresses(address, 8) == 0) {
		return -1;
	}

	if (cacheSystem->blockDataSize < 8) {
		cacheSystemByteWrite(cacheSystem, address, ID, (uint8_t) (data >> 32));
		cacheSystemByteWrite(cacheSystem, address + 4, ID, (uint8_t) (data & UINT32_MAX));
	}
	uint8_t array[8];
	array[0] = (uint8_t) (data >> 56);
	array[1] = (uint8_t) ((data >> 48) & UINT8_MAX);
	array[2] = (uint8_t) ((data >> 40) & UINT8_MAX);
	array[3] = (uint8_t) ((data >> 32) & UINT8_MAX);
	array[4] = (uint8_t) ((data >> 24) & UINT8_MAX);
	array[5] = (uint8_t) ((data >> 16) & UINT8_MAX);
	array[6] = (uint8_t) ((data >> 8) & UINT8_MAX);
	array[7] = (uint8_t) (data & UINT8_MAX);
	cacheSystemWrite(cacheSystem, address, ID, 8, array);
	return 0;
}
