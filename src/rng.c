#include "rng.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

// Mutex to protect the global random state from data races
static pthread_mutex_t rng_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize random number generator with the given seed
void rng_init(unsigned int seed) {
  pthread_mutex_lock(&rng_mutex);
  if (seed == 0) {
    seed = (unsigned)time(NULL);
  }
  srand(seed);
  pthread_mutex_unlock(&rng_mutex);
}

// Generate a random integer in the range [min, max]
int rng_range(int min, int max) {
  pthread_mutex_lock(&rng_mutex); // Lock before using rand()

  int res;
  if (min == max) {
    res = min;
  } else {
    if (min > max) {
      int temp = min;
      min = max;
      max = temp;
    }
    res = min + rand() % (max - min + 1);
  }

  pthread_mutex_unlock(&rng_mutex); // Unlock after
  return res;
}