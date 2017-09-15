/* Summer 2017 */
#include <stdbool.h>
#include <stdint.h>
#include "coherenceUtils.h"
#include "coherenceRead.h"
#include "../cache/utils.h"
#include "../cache/setInCache.h"
#include "../cache/getFromCache.h"
#include "../cache/cacheRead.h"
#include "../cache/cacheWrite.h"
#include "../cache/mem.h"
#include "../hitrate/hitRate.h"

/*
	A function which processes all cache reads for an entire cache system.
	Takes in a cache system, an address, an id for a cache, and a size to read
	and calls the appropriate functions on the cache being selected to read
	the data. Returns the data.
*/

// Assume that size <= blockDataSize, deal with that in the higher order functions
uint8_t* cacheSystemRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID, uint8_t size) {
	uint8_t* retVal;
	uint8_t offset;
	uint8_t* transferData;
	evictionInfo_t* dstCacheInfo;
	evictionInfo_t* otherCacheInfo;
	uint32_t evictionBlockNumber;
	cacheNode_t** caches;
	bool otherCacheContains = false;
	cache_t* dstCache = NULL;
	uint8_t counter = 0;
	caches = cacheSystem->caches;
	while (dstCache == NULL && counter < cacheSystem->size) { //Selects destination cache pointer from array of caches pointers
		if (caches[counter]->ID == ID) {
			dstCache = caches[counter]->cache;
		}
		counter++;
	}
	dstCacheInfo = findEviction(dstCache, address); //Finds block to evict and potential match
	evictionBlockNumber = dstCacheInfo->blockNumber;
	offset = getOffset(dstCache, address);

	// Check for valid address?

	if (dstCacheInfo->match) {
		/*What do you do if it is in the cache?*/
		/*Your Code Here*/
		enum state dstCacheState = determineState(dstCache, address);

		// Modified: up-to-date and only copy
		if (dstCacheState == MODIFIED || dstCacheState == EXCLUSIVE) {
			// need to return later
			retVal = readFromCache(dstCache, address, size);

		// Owner: up-to-date, other caches have a shared copy
		} else if (dstCacheState == OWNED) {
			// maybe don't have to check other caches
			retVal = readFromCache(dstCache, address, size);

		// May have to update retVal if another cache has more recent data
		} else if (dstCacheState == SHARED) {
			retVal = readFromCache(dstCache, address, size);
		}

	} else {
		uint32_t oldAddress = extractAddress(dstCache, extractTag(dstCache, evictionBlockNumber), evictionBlockNumber, 0);
		/*How do you need to update the snooper?*/
		/*How do you need to update states for what is getting evicted (don't worry about evicting this will be handled at a later step when you place data in the cache)?*/
		/*Your Code Here*/

		// need to remove Invalid block from snooper
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
				if (returnIDIf1(cacheSystem->snooper, oldAddress, cacheSystem->blockDataSize) == -1 && determineState(temp, oldAddress) == SHARED) {
					continue;
				}
				updateState(temp, oldAddress, INVALID);
			}
		}


		int val = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
		/*Check other caches???*/
		/*Your Code Here*/
		while (val != -1) {
			cache_t* other;
			for (int i = 0; i < cacheSystem->size; i++) {
				if (caches[i]->ID == val) {
					other = caches[i]->cache;
					break;
				}
			}
			otherCacheInfo = findEviction(other, address);
			enum state otherState = determineState(other, address);

			// Copy data from other cache, set current cache to SHARED
			if (otherCacheInfo->match) {
				otherCacheContains = true;
				if (otherState == SHARED || otherState == OWNED) {
					transferData = fetchBlock(other, otherCacheInfo->blockNumber);
					writeToCache(dstCache, address, transferData, cacheSystem->blockDataSize);
					free(otherCacheInfo);
					free(transferData);
					retVal = readFromCache(dstCache, address, size);
					break;

				} else if (otherState == EXCLUSIVE) {
					transferData = fetchBlock(other, otherCacheInfo->blockNumber);
					writeToCache(dstCache, address, transferData, cacheSystem->blockDataSize);
					free(otherCacheInfo);
					free(transferData);
					retVal = readFromCache(dstCache, address, size);
					break;
				} else if (otherState == MODIFIED) {
					transferData = fetchBlock(other, otherCacheInfo->blockNumber);
					writeToCache(dstCache, address, transferData, cacheSystem->blockDataSize);
					free(otherCacheInfo);
					free(transferData);
					retVal = readFromCache(dstCache, address, size);
					break;
				}

			// Not sure if we have to clear other cache from snooper if match is a miss
			} else {
				if (otherState == INVALID) {
					removeFromSnooper(cacheSystem->snooper, address, val, other->blockDataSize);
					evict(other, otherCacheInfo->blockNumber);
					setState(other, otherCacheInfo->blockNumber, INVALID);
					uint32_t addrTag = getTag(other, address);
					uint32_t addrIndex = getIndex(other, address);
					oldLRU = otherCacheInfo->LRU;
					decrementLRU(other, addrTag, addrIndex, oldLRU);
					free(otherCacheInfo);
				}
			}
			val = returnFirstCacheID(cacheSystem->snooper, address, cacheSystem->blockDataSize);
		}

		if (!otherCacheContains) {
			/*Check Main memory?*/
			/*Your Code Here*/

			// If block is not present, readFromCache should readFromMem and write to the cache
			// block should now be exclusive
			retVal = readFromMem(dstCache, address);
			writeToCache(dstCache, address, retVal, cacheSystem->blockDataSize);
			setState(dstCache, evictionBlockNumber, EXCLUSIVE);
		}

	}
	addToSnooper(cacheSystem->snooper, address, ID, cacheSystem->blockDataSize);
	if (otherCacheContains) {
		/*What states need to be updated?*/
		/*Your Code Here*/
		enum state newState;
		if (dstCacheInfo->match) {
			newState = determineState(dstCache, address);
		} else {
			if (returnIDIf1(cacheSystem->snooper, address, cacheSystem->blockDataSize) == ID) {
				setState(dstCache, evictionBlockNumber, EXCLUSIVE);
				newState = EXCLUSIVE;
			} else if (determineState(dstCache, address) == OWNED) {
				newState = OWNED;
			} else {
				setState(dstCache, evictionBlockNumber, SHARED);
				newState = SHARED;
			}
		} 
		

		for (int i = 0; i < cacheSystem->size; i++) {
			if (caches[i]->ID != ID && snooperContains(cacheSystem->snooper, address, caches[i]->ID)) {
				dstCache = caches[i]->cache;
				updateState(dstCache, address, newState);
			}
		}
	}
	free(dstCacheInfo);
	return retVal;
}

/*
	A function used to request a byte from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
byteInfo_t cacheSystemByteRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	byteInfo_t retVal;
	if (!cacheSystem) {
		retVal.success = false;
		return retVal;
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
		retVal.success = false;
		return retVal;
	}

	if (validAddresses(address, 1) == 0) {
		retVal.success = false;
	} else {
		uint8_t* data = cacheSystemRead(cacheSystem, address, ID, 1);
		retVal.data = data[0];
		retVal.success = true;
		free(data);
	}
	return retVal;
}

/*
	A function used to request a halfword from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
halfWordInfo_t cacheSystemHalfWordRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	halfWordInfo_t retVal;
	if (!cacheSystem) {
		retVal.success = false;
		return retVal;
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
		retVal.success = false;
		return retVal;
	}


	if (validAddresses(address, 2) == 0 || address % 2 != 0) {
		retVal.success = false;
	} else {
		byteInfo_t temp;
		retVal.data = 0;
		if (cacheSystem->blockDataSize < 2) {
			for (int i = 0; i < 2; i++) {
				temp = cacheSystemByteRead(cacheSystem, address, ID);
				retVal.data += ((uint16_t) temp.data << (8 * (1 - i)));
				address++;
			}
		} else {
			uint8_t* data = cacheSystemRead(cacheSystem, address, ID, 2);
			for (int i = 0; i < 2; i ++) {
				retVal.data += ((uint16_t) data[i] << (8 * (1 - i)));
			}
			free(data);
		}
		retVal.success = true;
	}
	return retVal;
}


/*
	A function used to request a word from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
wordInfo_t cacheSystemWordRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	wordInfo_t retVal;
	if (!cacheSystem) {
		retVal.success = false;
		return retVal;
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
		retVal.success = false;
		return retVal;
	}

	if (validAddresses(address, 4) == 0 || address % 4 != 0) {
		retVal.success = false;
	} else {
		halfWordInfo_t temp;
		retVal.data = 0;
		if (cacheSystem->blockDataSize < 4) {
			for (int i = 0; i < 2; i++) {
				temp = cacheSystemHalfWordRead(cacheSystem, address, ID);
				retVal.data += ((uint32_t) temp.data << (8 * (3 - i)));
				address += 2;
			}
		} else {
			uint8_t* data = cacheSystemRead(cacheSystem, address, ID, 4);
			for (int i = 0; i < 4; i ++) {
				retVal.data += ((uint32_t) data[i] << (8 * (3 - i)));
			}
			free(data);
		}
		retVal.success = true;
	}
	return retVal;
}

/*
	A function used to request a doubleword from a specific cache in a cache system.
	Takes in a cache system, an address, and an ID for the cache which will be
	read from. Returns a struct with the data and a bool field indicating
	whether or not the read was a success.
*/
doubleWordInfo_t cacheSystemDoubleWordRead(cacheSystem_t* cacheSystem, uint32_t address, uint8_t ID) {
	doubleWordInfo_t retVal;
	if (!cacheSystem) {
		retVal.success = false;
		return retVal;
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
		retVal.success = false;
		return retVal;
	}

	if (validAddresses(address, 8) == 0 || address % 8 != 0) {
		retVal.success = false;
	} else {
		wordInfo_t temp;
		retVal.data = 0;
		if (cacheSystem->blockDataSize < 8) {
			for (int i = 0; i < 2; i++) {
				temp = cacheSystemWordRead(cacheSystem, address, ID);
				retVal.data += ((uint64_t) temp.data << (8 * (7 - i)));
				address += 4;
			}
		} else {
			uint8_t* data = cacheSystemRead(cacheSystem, address, ID, 8);
			for (int i = 0; i < 8; i ++) {
				retVal.data += ((uint64_t) data[i] << (8 * (7 - i)));
			}
			free(data);
		}
		retVal.success = true;
	}
	return retVal;
}
