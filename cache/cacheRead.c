/* Summer 2017 */
#include <stdbool.h>
#include <stdint.h>
#include "utils.h"
#include "setInCache.h"
#include "cacheRead.h"
#include "cacheWrite.h"
#include "getFromCache.h"
#include "mem.h"
#include "../hitrate/hitRate.h"

/*
	Takes in a cache and a block number and fetches that block of data,
	returning it in a uint8_t* pointer.
*/
uint8_t* fetchBlock(cache_t* cache, uint32_t blockNumber) {
	uint64_t location = getDataLocation(cache, blockNumber, 0);
	uint32_t length = cache->blockDataSize;
	uint8_t* data = malloc(sizeof(uint8_t) << log_2(length));
	if (data == NULL) {
		allocationFailed();
	}
	int shiftAmount = location & 7;
	uint64_t byteLoc = location >> 3;
	if (shiftAmount == 0) {
		for (uint32_t i = 0; i < length; i++) {
			data[i] = cache->contents[byteLoc + i];
		}
	} else {
		length = length << 3;
		data[0] = cache->contents[byteLoc] << shiftAmount;
		length -= (8 - shiftAmount);
		int displacement = 1;
		while (length > 7) {
			data[displacement - 1] = data[displacement - 1] | (cache->contents[byteLoc + displacement] >> (8 - shiftAmount));
			data[displacement] = cache->contents[byteLoc + displacement] << shiftAmount;
			displacement++;
			length -= 8;
		}
		data[displacement - 1] = data[displacement - 1] | (cache->contents[byteLoc + displacement] >> (8 - shiftAmount));
	}
	return data;
}

/*
	Takes in a cache, an address, and a dataSize and reads from the cache at
	that address the number of bytes indicated by the size. If the data block
	is already in the cache it retrieves the contents. If the contents are not
	in the cache it is read into a new slot and if necessary something is
	evicted.
*/
uint8_t* readFromCache(cache_t* cache, uint32_t address, uint32_t dataSize) {
	if (validAddresses(address, dataSize) == 0) {
		return NULL;
	}

	// split into 2 adjacent blocks 0, 1
	// block 0 -> address
	// block 1 -> address + 1

	uint32_t blockNumber = (cache->totalDataSize) / (cache->blockDataSize);
    uint32_t addrIndex = getIndex(cache, address);
    uint32_t addrTag = getTag(cache, address);
    uint32_t addrOffset = getOffset(cache, address);
    bool blockFound = false;

    uint8_t* data;
    for (uint32_t i = 0; i < blockNumber; i++) {
    	if (getValid(cache, i) == 0) {
    		continue;
    	}
        if (addrIndex == extractIndex(cache, i)) {
            //if data is in the cache already
            if (addrTag == extractTag(cache, i)) {
				reportHit(cache);
				/*
				if (cache->blockDataSize < dataSize) {
					uint32_t numDataBlocks = (dataSize / cache->blockDataSize);
					uint32_t remainingBytes = (dataSize % cache->blockDataSize);
					data = (uint8_t*) malloc(sizeof(uint8_t) * dataSize);

					for (int i = 0; i < numDataBlocks; i++) {
						addrOffset = getOffset(cache, address);
						uint8_t* temp = getData(cache, addrOffset, i, cache->blockDataSize);
						for (int k = 0; k < cache->blockDataSize; k++) {
							data[i*cache->blockDataSize + k] = temp[k];
						}
						address += cache->blockDataSize;
						free(temp);
					}
					addrOffset = getOffset(cache, address);
					temp = getData(cache, addrOffset, i, remainingBytes);

					for (int i = 0; i < remainingBytes; i++) {
						data[numDataBlocks*cache->blockDataSize + i] = temp[i];
					}
					free(temp);

				} else {
	               	data = getData(cache, addrOffset, i, dataSize);
					updateLRU(cache, addrTag, addrIndex, getLRU(cache, i));
				}
				*/
				data = getData(cache, addrOffset, i, dataSize);
				updateLRU(cache, addrTag, addrIndex, getLRU(cache, i));
				blockFound = true;
                break;
            }
        }
    }

    // Need to update LRU... where?
    if (!blockFound) {
		/*
    	if (cache->blockDataSize < dataSize) {
    		uint32_t numDataBlocks = (dataSize / cache->blockDataSize);
			uint32_t remainingBytes = (dataSize % cache->blockDataSize);

			if (remainingBytes != 0) {
				numDataBlocks += 1;
			}

			data = (uint8_t*) malloc(sizeof(uint8_t) * dataSize);
			uint8_t* temp;

			for (int i = 0; i < numDataBlocks; i++) {
				evictionInfo_t* toBeEvicted = findEviction(cache, address);
				uint32_t evictBlockNum = toBeEvicted->blockNumber;
				evict(cache, toBeEvicted->blockNumber);
				uint32_t addr = extractAddress(cache, getTag(cache, address), evictBlockNum, 0);
				temp = readFromMem(cache, addr);
				writeDataToCache(cache, addr, temp, cache->blockDataSize, getTag(cache, address), toBeEvicted);
				free(temp);
				setDirty(cache, evictBlockNum, 0);
				setTag(cache, addrTag, evictBlockNum);
				setLRU(cache, evictBlockNum, 0);
			 	temp = getData(cache, addrOffset, evictBlockNum, cache->blockDataSize);
			 	for (int k = 0; k < cache->blockDataSize; k++) {
			 		data[i*cache->blockDataSize + k] = temp[k];
			 	}
			 	free(temp);
			 	free(toBeEvicted);
			 	address += cache->blockDataSize;
			}

			if (remainingBytes > 0) {
				temp = getData(cache, addrOffset, evictBlockNum, cache->blockDataSize);
				for (int i = 0; i < remainingBytes; i++) {
					data[(numDataBlocks - 1)*cache->blockDataSize + i] = temp[i];
				}
				free(temp);
			}
    	} else {
		*/
	    	evictionInfo_t* toBeEvicted = findEviction(cache, address);
	    	uint32_t evictBlockNum = toBeEvicted->blockNumber;
			evict(cache, toBeEvicted->blockNumber);
			uint32_t addr = extractAddress(cache, getTag(cache, address), evictBlockNum, 0);
			data = readFromMem(cache, addr);
			writeDataToCache(cache, addr, data, cache->blockDataSize, getTag(cache, address), toBeEvicted);
			free(data);
			setDirty(cache, evictBlockNum, 0);
			setTag(cache, addrTag, evictBlockNum);
			setLRU(cache, evictBlockNum, 0);
		 	data = getData(cache, addrOffset, evictBlockNum, dataSize);
			free(toBeEvicted);

   }
	return data;

}

/*
	Takes in a cache and an address and fetches a byte of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
byteInfo_t readByte(cache_t* cache, uint32_t address) {
	byteInfo_t retVal;

	if (validAddresses(address, 1) == 0) {
		retVal.success = false;
	} else {
		uint8_t* _data = readFromCache(cache, address, 1);
		retVal.data = *(_data);
		retVal.success = true;
		free(_data);
		reportAccess(cache);
	}
	return retVal;
}

/*
	Takes in a cache and an address and fetches a halfword of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
halfWordInfo_t readHalfWord(cache_t* cache, uint32_t address) {
	halfWordInfo_t retVal;

	if (validAddresses(address, 2) == 0 || address % 2 != 0) {
		retVal.success = false;
	} else {
		retVal.data = 0;
		if (cache->blockDataSize < 2) {
			uint8_t numBlocks = (2 / cache->blockDataSize);
			for (int i = 0; i < numBlocks; i++) {
				byteInfo_t temp = readByte(cache, address);
				retVal.data += ((uint32_t) temp.data << (8 * (1 - i)));
				address++;
			}
		} else {
			uint8_t* _data = readFromCache(cache, address, 2);
			reportAccess(cache);
			for (int i = 0; i < 2; i++) {
				retVal.data += ((uint16_t) _data[i] << (8 * (1 - i)));
			}
			free(_data);
		}
		retVal.success = true;
	}

	return retVal;
}

/*
	Takes in a cache and an address and fetches a word of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
wordInfo_t readWord(cache_t* cache, uint32_t address) {
	wordInfo_t retVal;
	if (validAddresses(address, 4) == 0 || address % 4 != 0) {
		retVal.success = false;
	} else {
		retVal.data = 0;
		if (cache->blockDataSize < 4) {
			for (int i = 0; i < 2; i++) {
				halfWordInfo_t temp = readHalfWord(cache, address);
				retVal.data += ((uint32_t) temp.data << (16 * (1 - i)));
				address += 2;
			}
		} else {
			uint8_t* _data = readFromCache(cache, address, 4);
			reportAccess(cache);
			for (int i = 0; i < 4; i++) {
				retVal.data += ((uint32_t) _data[i] << (8 * (3- i)));
			}
			free(_data);
		}
		retVal.success = true;
	}

	return retVal;
}

/*
	Takes in a cache and an address and fetches a double word of data.
	Returns a struct containing a bool field of whether or not
	data was successfully read and the data. This field should be
	false only if there is an alignment error or there is an invalid
	address selected.
*/
doubleWordInfo_t readDoubleWord(cache_t* cache, uint32_t address) {
	doubleWordInfo_t retVal;
	if (validAddresses(address, 8) == 0 || address % 8 != 0) {
		retVal.success = false;
	} else {
		retVal.data = 0;
		if (cache->blockDataSize < 8) {
			for (int i = 0; i < 2; i++) {
				wordInfo_t temp = readWord(cache, address);
				retVal.data += (uint64_t) ((uint64_t) temp.data << (uint64_t) (32 * (1 - i)));
				address += 4;
			}
		} else {
			uint8_t* _data = readFromCache(cache, address, 8);
			reportAccess(cache);
			for (int i = 0; i < 8; i++) {
				retVal.data += (uint64_t) ((uint64_t) _data[i] << (uint64_t) (8 * (7 - i)));
			}
			free(_data);
		}
		retVal.success = true;
	}
	return retVal;
}
