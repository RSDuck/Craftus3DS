#ifndef RANDOM_H_INCLUDED

#include <stdint.h>

inline uint32_t xorshift32(uint32_t x32) {
	x32 ^= x32 << 13;
	x32 ^= x32 >> 17;
	x32 ^= x32 << 5;
	return x32;
}

inline uint32_t randN(uint32_t* seed, uint32_t max) {
	*seed = xorshift32(*seed);
	return *seed % max;
}

#endif  // !RANDOM_H_INCLUDED