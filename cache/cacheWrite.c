/* Summer 2017 */
#include <stdbool.h>
#include <stdint.h>
#include "utils.h"
#include "cacheWrite.h"
#include "getFromCache.h"
#include "mem.h"
#include "setInCache.h"
#include "../hitrate/hitRate.h"

/*
	Takes in a cache and a block number and evicts the block at that number
	from the cache. This does not change any of the bits in the cache but
	checks if data needs to be written to main memory or and then makes
	calls to the appropriate functions to do so.
*/
void evict(cache_t* cache, uint32_t blockNumber) {
	uint8_t valid = getValid(cache, blockNumber);
	uint8_t dirty = getDirty(cache, blockNumber);
	if (valid && dirty) {
		uint32_t tag = extractTag(cache, blockNumber);
		uint32_t address = extractAddress(cache, tag, blockNumber, 0);
		writeToMem(cache, blockNumber, address);
	}
}

/*
	Takes in a cache, an address, a pointer to data, and a size of data
	and writes the updated data to the cache. If the data block is already
	in the cache it updates the contents and sets the dirty bit. If the
	contents are not in the cache it is written to a new slot and
	if necessary something is evicted from the cache.
*/
void writeToCache(cache_t* cache, uint32_t address, uint8_t* data, uint32_t dataSize) {
    uint32_t blockNumber = (cache->totalDataSize) / (cache->blockDataSize);
    uint32_t addrIndex = getIndex(cache, address);
    uint32_t addrTag = getTag(cache, address);
    bool blockFound = false;

    for (uint32_t i = 0; i < blockNumber; i++) {
        if (addrIndex == extractIndex(cache, i)) {
            if (addrTag == extractTag(cache, i)) {
				reportHit(cache);
                evictionInfo_t* toBeEvicted = findEviction(cache, address);
				writeDataToCache(cache, address, data, dataSize, addrTag, toBeEvicted);
				blockFound = true;
				free(toBeEvicted);
	            break;
            }
        }
    }

    if (!blockFound) {
        evictionInfo_t* toBeEvicted = findEviction(cache, address);
        uint32_t evictBlockNum = toBeEvicted->blockNumber;
        evict(cache, toBeEvicted->blockNumber);
		setDirty(cache, evictBlockNum, 0);
		uint32_t addr = extractAddress(cache, getTag(cache, address), evictBlockNum, 0);
		uint8_t* toWrite = readFromMem(cache, addr);
		writeDataToCache(cache, addr, toWrite, cache->blockDataSize, getTag(cache, address), toBeEvicted);
		setData(cache, data, evictBlockNum, dataSize, getOffset(cache, address));
		setTag(cache, addrTag, evictBlockNum);
		setLRU(cache, evictBlockNum, 0);
		free(toBeEvicted);
		free(toWrite);
    }
}

/*
	Takes in a cache, an address to write to, a pointer containing the data
	to write, the size of the data, a tag, and a pointer to an evictionInfo
	struct and writes the data given to the cache based upon the location
	given by the evictionInfo struct.
*/
void writeDataToCache(cache_t* cache, uint32_t address, uint8_t* data, uint32_t dataSize, uint32_t tag, evictionInfo_t* evictionInfo) {
	uint32_t idx = getIndex(cache, address);
	setData(cache, data, evictionInfo->blockNumber, dataSize , getOffset(cache, address));
	setDirty(cache, evictionInfo->blockNumber, 1);
	setValid(cache, evictionInfo->blockNumber, 1);
	setShared(cache, evictionInfo->blockNumber, 0);
	updateLRU(cache, tag, idx, evictionInfo->LRU);
}

/*
	Takes in a cache, an address, and a byte of data and writes the byte
	of data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns -1
	if the address is invalid, otherwise 0.
*/
int writeByte(cache_t* cache, uint32_t address, uint8_t data) {
    if (validAddresses(address, 1) == 0) {
        return -1;
    }
    writeToCache(cache, address, &data, 1);
	reportAccess(cache);
	return 0;
}

/*
	Takes in a cache, an address, and a halfword of data and writes the
	data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns 0
	for a success and -1 if there is an allignment error or an invalid
	address was used.
*/
int writeHalfWord(cache_t* cache, uint32_t address, uint16_t data) {
    if (validAddresses(address, 1) == 0 || address % 2 != 0) {
        return -1;
    }
	uint8_t toPass[2];
	toPass[0] = data >> 8;
	toPass[1] = data & 0x000000FF;
	if (cache->blockDataSize < 2) {
		writeByte(cache, address, toPass[0]);
		writeByte(cache, address + 1, toPass[1]);
	} else {
		reportAccess(cache);
	    writeToCache(cache, address, toPass, 2);
	}
    return 0;
}

/*
	Takes in a cache, an address, and a word of data and writes the
	data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns 0
	for a success and -1 if there is an allignment error or an invalid
	address was used.
*/
int writeWord(cache_t* cache, uint32_t address, uint32_t data) {
    if (validAddresses(address, 1) == 0 || address % 4 != 0) {
        return -1;
    }
	uint8_t toPass[4];
	toPass[0] = data >> 24;
	toPass[1] = (data & 0x00FF0000) >> 16;
	toPass[2] = (data & 0x0000FF00) >> 8;
	toPass[3] = (data & 0x000000FF);
	if (cache->blockDataSize < 4) {
		uint16_t toPassHalf[2];
		toPassHalf[0] = data >> 16;
		toPassHalf[1] = (data & 0x0000FFFF);
		writeHalfWord(cache, address, toPassHalf[0]);
		writeHalfWord(cache, address + 2, toPassHalf[1]);
	} else {
		reportAccess(cache);
    	writeToCache(cache, address, toPass, 4);
	}
    return 0;
}

/*
	Takes in a cache, an address, and a double word of data and writes the
	data to the cache. May evict something if the block is not already
	in the cache which may also require a fetch from memory. Returns 0
	for a success and -1 if there is an allignment error or an invalid address
	was used.
*/
int writeDoubleWord(cache_t* cache, uint32_t address, uint64_t data) {
    if (validAddresses(address, 1) == 0 || address % 8 != 0) {
        return -1;
    }
	uint8_t toPass[8];
	toPass[0] = data >> 56;
	toPass[1] = (data & 0x00FF000000000000) >> 48;
	toPass[2] = (data & 0x0000FF0000000000) >> 40;
	toPass[3] = (data & 0x000000FF00000000) >> 32;
	toPass[4] = (data & 0x00000000FF000000) >> 24;
	toPass[5] = (data & 0x0000000000FF0000) >> 16;
	toPass[6] = (data & 0x000000000000FF00) >> 8;
	toPass[7] = (data & 0x00000000000000FF);

	if (cache->blockDataSize < 8) {
		uint32_t toPassHalf[2];
		toPassHalf[0] = data >> 32;
		toPassHalf[1] = (data & 0x00000000FFFFFFFF);
		writeWord(cache, address, toPassHalf[0]);
		writeWord(cache, address + 4, toPassHalf[1]);
	} else {
		reportAccess(cache);
    	writeToCache(cache, address, toPass, 8);
	}
    return 0;
}

/*
	A function used to write a whole block to a cache without pulling it from
	physical memory. This is useful to transfer information between caches
	without needing to take an intermediate step of going through main memory,
	a primary advantage of MOESI. Takes in a cache to write to, an address
	which is being written to, the block number that the data will be written
	to and an entire block of data from another cache.
*/
void writeWholeBlock(cache_t* cache, uint32_t address, uint32_t evictionBlockNumber, uint8_t* data) {
	uint32_t idx = getIndex(cache, address);
	uint32_t tagVal = getTag(cache, address);
	int oldLRU = getLRU(cache, evictionBlockNumber);
	evict(cache, evictionBlockNumber);
	setValid(cache, evictionBlockNumber, 1);
	setDirty(cache, evictionBlockNumber, 0);
	setTag(cache, tagVal, evictionBlockNumber);
	setData(cache, data, evictionBlockNumber, cache->blockDataSize, 0);
	updateLRU(cache, tagVal, idx, oldLRU);
}
