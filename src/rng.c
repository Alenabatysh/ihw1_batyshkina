#include "rng.h"
#include <stdlib.h>
#include <time.h>

// Initialize random number generator with the given seed
// If seed is 0, use current time as seed
void rng_init(unsigned int seed) {
  if (seed == 0) {
    seed = (unsigned)time(NULL);
  }
  srand(seed);
}

// Generate a random integer in the range [min, max]
int rng_range(int min, int max) {
  // If range is a single value, return it directly
  if (min == max) {
    return min;
  }
  // Swap min and max if they are in wrong order
  if (min > max) {
    int temp = min;
    min = max;
    max = temp;
  }
  int res = min + rand() % (max - min + 1);
  return res;
}