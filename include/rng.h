#ifndef RNG_H
#define RNG_H

// Initialize random number generator with the given seed
// If seed is 0, current time is used
void rng_init(unsigned int seed);

// Generate a random integer in the range [min, max]
int rng_range(int min, int max);

#endif