#pragma once

#include <cstdint>

struct BitEdit {

	static uint32_t readU32(uint32_t* data, uint64_t bitOffset, uint64_t bitCount) {
		if (bitCount == 0 || bitCount > 32) return 0;

		uint64_t wordIdx = bitOffset / 32;
		uint32_t bitShift = bitOffset % 32;

		// Load the primary word
		uint64_t combined = data[wordIdx];

		// If the bit range spans into the next word, pull it in
		if (bitShift + bitCount > 32) {
			combined |= (uint64_t(data[wordIdx + 1]) << 32);
		}

		// Shift away the offset and mask the requested bitCount
		uint32_t mask = (bitCount == 32) ? 0xFFFFFFFF : (1U << bitCount) - 1;

		return uint32_t((combined >> bitShift) & mask);
	}

	static void writeU32(uint32_t* data, uint64_t bitOffset, uint64_t bitCount, uint32_t value) {
		if (bitCount == 0 || bitCount > 32) return;

		uint64_t wordIdx = bitOffset / 32;
		uint32_t bitShift = bitOffset % 32;
		uint32_t mask = (bitCount == 32) ? 0xFFFFFFFF : (1U << bitCount) - 1;
		
		// Ensure value doesn't exceed bitCount
		value &= mask;

		// Handle first word
		uint32_t firstMask = ~(mask << bitShift);
		data[wordIdx] = (data[wordIdx] & firstMask) | (value << bitShift);

		// Handle second word if straddling boundary
		if (bitShift + bitCount > 32) {
			uint32_t bitsWritten = 32 - bitShift;
			uint32_t secondMask = ~(mask >> bitsWritten);
			data[wordIdx + 1] = (data[wordIdx + 1] & secondMask) | (value >> bitsWritten);
		}
	}
};