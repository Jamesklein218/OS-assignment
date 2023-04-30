#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#ifndef MLQ_SCHED
#define MLQ_SCHED
#endif

#define MAX_PRIO 139

int queue_empty (void);

/* Initialize the scheduler */
void init_scheduler (void);

/* */
void finish_scheduler (void);

/* Get the next process from ready queue */
struct pcb_t *get_proc (void);

/* Put a process back to run queue */
void put_proc (struct pcb_t *proc);

/* Add a new process to ready queue */
void add_proc (struct pcb_t *proc);

/* For MLQ_SCHED only,
 * use to decrease the maximum slot of each queue
 */
void decrease_q_time_left ();

/* For MLQ_SCHED only,
 * check if the queue's time_left equals 0
 */
int queue_time_up ();

#endif
