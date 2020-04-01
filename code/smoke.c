#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}
/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

//
// TODO
// You will probably need to add some procedures and struct etc.
//
int match_in_hand = 0, paper_in_hand = 0, tobacco_in_hand = 0;

void* paper_smoker(void *av) {
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1) {
    uthread_cond_wait(a->paper);
    if (match_in_hand > 0 && tobacco_in_hand > 0) {
      match_in_hand -= 1;
      tobacco_in_hand -= 1;
      smoke_count[PAPER] += 1;
      uthread_cond_signal(a->smoke);
    } else {
      paper_in_hand += 1;
    }
    if (paper_in_hand > 0 && tobacco_in_hand > 0) {
      uthread_cond_signal(a->match);
    } else if (match_in_hand > 0 && paper_in_hand > 0) {
      uthread_cond_signal(a->tobacco);
    } else if (match_in_hand > 0 && tobacco_in_hand > 0) {
      uthread_cond_signal(a->paper);
    }
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

void* tobacco_smoker(void *av) {
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1) {
    uthread_cond_wait(a->tobacco);
    if (match_in_hand > 0 && paper_in_hand > 0) {
      match_in_hand -= 1;
      paper_in_hand -= 1;
      smoke_count[TOBACCO] += 1;
      uthread_cond_signal(a->smoke);
    } else {
      tobacco_in_hand += 1;
    }
    if (paper_in_hand > 0 && tobacco_in_hand > 0) {
      uthread_cond_signal(a->match);
    } else if (match_in_hand > 0 && paper_in_hand > 0) {
      uthread_cond_signal(a->tobacco);
    } else if (match_in_hand > 0 && tobacco_in_hand > 0) {
      uthread_cond_signal(a->paper);
    }
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

void* match_smoker(void *av) {
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1) {
    uthread_cond_wait(a->match);
    if (tobacco_in_hand > 0 && paper_in_hand > 0) {
      paper_in_hand -= 1;
      tobacco_in_hand -= 1;
      smoke_count[MATCH] += 1;
      uthread_cond_signal(a->smoke);
    } else {
      match_in_hand += 1;
    }
    if (paper_in_hand > 0 && tobacco_in_hand > 0) {
      uthread_cond_signal(a->match);
    } else if (match_in_hand > 0 && paper_in_hand > 0) {
      uthread_cond_signal(a->tobacco);
    } else if (match_in_hand > 0 && tobacco_in_hand > 0) {
      uthread_cond_signal(a->paper);
    }
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};
  
  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

int main (int argc, char** argv) {
  uthread_init (7);
  struct Agent*  a = createAgent();

  uthread_t match, paper, tobacco, agnt;

  match = uthread_create(match_smoker, a);
  tobacco = uthread_create(tobacco_smoker, a);
  paper = uthread_create(paper_smoker, a);
  agnt = uthread_create(agent, a);

  uthread_join(agnt, NULL);

  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}