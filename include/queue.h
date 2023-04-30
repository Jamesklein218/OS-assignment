
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define MAX_QUEUE_SIZE 10

struct queue_t
{
  struct pcb_t **proc;
  int size;
  int time_left;
  int capacity;
};

/* Put a new process to queue [q] */
void enqueue (struct queue_t *q, struct pcb_t *proc);

/* Return a pcb on top of the queue [q] and remove it from q
 * */
struct pcb_t *dequeue (struct queue_t *q);

/*
 * Check if the queue is empty, return 0(for false) and 1(for true)
 * */
int empty (struct queue_t *q);

/*
 * Private function for resizing the queue to the desired value and reallocate
 * capacity if needed
 * */
void resize (struct queue_t *q, int new_size);

#endif
