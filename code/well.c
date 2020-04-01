#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

#define MAX_OCCUPANCY      3
#define NUM_ITERATIONS     100
#define NUM_PEOPLE         20
#define FAIR_WAITING_COUNT 4

// constraints:
// people of opposing endianness can never be at the well at the same time
// no more than 3 people can be in the well area at the same time

/**
 * You might find these declarations useful.
 */
enum Endianness {LITTLE = 0, BIG = 1};
const static enum Endianness oppositeEnd [] = {BIG, LITTLE};

struct Well {
  // TODO
  uthread_mutex_t mutex;
  uthread_cond_t little;
  uthread_cond_t big;
  int count;
  int time_elapsed;
  enum Endianness current_endianness;
};

struct Well* createWell() {
  struct Well* Well = malloc (sizeof (struct Well));
  // TODO
  Well->mutex = uthread_mutex_create();
  Well->little = uthread_cond_create(Well->mutex);
  Well->big = uthread_cond_create(Well->mutex);
  Well->count = 0;
  Well->time_elapsed = 0;
  Well->current_endianness = 0;
  return Well;
}

struct Well* Well;

#define WAITING_HISTOGRAM_SIZE (NUM_ITERATIONS * NUM_PEOPLE)
int             entryTicker;                                          // incremented with each entry
int             waitingHistogram         [WAITING_HISTOGRAM_SIZE];
int             waitingHistogramOverflow;
uthread_mutex_t waitingHistogrammutex;
int             occupancyHistogram       [2] [MAX_OCCUPANCY + 1];


void recordWaitingTime (int waitingTime) {
  uthread_mutex_lock (waitingHistogrammutex);
  if (waitingTime < WAITING_HISTOGRAM_SIZE)
    waitingHistogram [waitingTime] ++;
  else
    waitingHistogramOverflow ++;
  uthread_mutex_unlock (waitingHistogrammutex);
}

void signalWell(enum Endianness g){
  if (g == BIG) {
      uthread_cond_signal(Well->big);
    } else if (g == LITTLE) {
      uthread_cond_signal(Well->little);
    }
}

void waitWell(enum Endianness g) {
  if (g == BIG) {
      uthread_cond_wait(Well->big);
    } else if (g == LITTLE) {
      uthread_cond_wait(Well->little);
    }
}

void enterWell (enum Endianness g) {
  // TODO
  uthread_mutex_lock(Well->mutex);
  if (Well->count == 0) {
    Well->current_endianness = g;
  } else {
    int time_of_entry = Well->time_elapsed;
    waitWell(g);
    int waiting_time = Well->time_elapsed - time_of_entry;
    recordWaitingTime(waiting_time);
  }
  Well->count += 1;
  Well->time_elapsed += 1;
  occupancyHistogram[g][Well->count] += 1;
  entryTicker += 1;
  uthread_mutex_unlock(Well->mutex);
}

void leaveWell() {
  // TODO
  uthread_mutex_lock(Well->mutex);
  Well->count -= 1;
  occupancyHistogram[Well->current_endianness][Well->count] += 1;
  if (Well->count == 0) {
    Well->current_endianness = oppositeEnd[Well->current_endianness];
    for (int i = 0; i < MAX_OCCUPANCY; i++) {
      signalWell(Well->current_endianness);
    }
  } else if (Well->time_elapsed > FAIR_WAITING_COUNT && Well->count < MAX_OCCUPANCY) {
    signalWell(Well->current_endianness);
  }
  uthread_mutex_unlock(Well->mutex);
}

//
// TODO
// You will probably need to create some additional produres etc.
//

void* well(void* wv) {
  enum Endianness g = random() % 2;
  for (int i = 0; i < NUM_ITERATIONS; i++) {
    enterWell(g);
    for (int j = 0; j < NUM_PEOPLE; j++) {
      uthread_yield();
    }
    leaveWell();
    for (int k = 0; k < NUM_PEOPLE; k++) {
      uthread_yield();
    }
  }
  return NULL;
}

int main (int argc, char** argv) {
  uthread_init (1);
  Well = createWell();
  uthread_t pt [NUM_PEOPLE];
  waitingHistogrammutex = uthread_mutex_create ();

  // TODO
  for (int i = 0; i < NUM_PEOPLE; i++) {
    pt[i] = uthread_create(well, Well);
  }

  for (int i = 0; i < NUM_PEOPLE; i++) {
    uthread_join(pt[i], NULL);
  }

  printf ("Times with 1 little endian %d\n", occupancyHistogram [LITTLE]   [1]);
  printf ("Times with 2 little endian %d\n", occupancyHistogram [LITTLE]   [2]);
  printf ("Times with 3 little endian %d\n", occupancyHistogram [LITTLE]   [3]);
  printf ("Times with 1 big endian    %d\n", occupancyHistogram [BIG] [1]);
  printf ("Times with 2 big endian    %d\n", occupancyHistogram [BIG] [2]);
  printf ("Times with 3 big endian    %d\n", occupancyHistogram [BIG] [3]);
  printf ("Waiting Histogram\n");
  for (int i=0; i<WAITING_HISTOGRAM_SIZE; i++)
    if (waitingHistogram [i])
      printf ("  Number of times people waited for %d %s to enter: %d\n", i, i==1?"person":"people", waitingHistogram [i]);
  if (waitingHistogramOverflow)
    printf ("  Number of times people waited more than %d entries: %d\n", WAITING_HISTOGRAM_SIZE, waitingHistogramOverflow);
}
