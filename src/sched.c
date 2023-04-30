
#include "os-cfg.h"
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>

static struct queue_t ready_queue; // deprecate
static struct queue_t run_queue;   // deprecate
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static unsigned int curr_prio;
#endif

int
queue_empty (void)
{
#ifdef MLQ_SCHED
  unsigned long prio;
  for (prio = 0; prio < MAX_PRIO; prio++)
    if (!empty (&mlq_ready_queue[prio]))
      return 0;

  return 1;
#endif
  return (empty (&ready_queue) && empty (&run_queue));
}

void
init_scheduler (void)
{
#ifdef MLQ_SCHED
  curr_prio = 0;
  int i;

  for (i = 0; i < MAX_PRIO; i++)
    {
      /* Initialize multilevel queue */
      mlq_ready_queue[i].size = 0;
      mlq_ready_queue[i].capacity = MAX_QUEUE_SIZE;
      mlq_ready_queue[i].proc = (struct pcb_t **)malloc (
          mlq_ready_queue[i].capacity * sizeof (struct pcb_t *));
      mlq_ready_queue[i].time_left = MAX_PRIO - i;
    }
#endif
  ready_queue.size = 0;
  ready_queue.capacity = MAX_QUEUE_SIZE;
  run_queue.size = 0;
  run_queue.size = MAX_QUEUE_SIZE;
  pthread_mutex_init (&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/*
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO -
 * prio)
 */
int
queue_time_up ()
{
  return mlq_ready_queue[curr_prio].time_left == 0;
}

struct pcb_t *
get_mlq_proc (void)
{
  struct pcb_t *proc = NULL;
  /* Get a process from PRIORITY [mlq_ready_queue].
   * Remember to use lock to protect the queue.
   * */
  pthread_mutex_lock (&queue_lock);

  /* Check if the ENTIRE MULTI-QUEUE is empty */
  if (queue_empty ())
    return proc;
  while (empty (&mlq_ready_queue[curr_prio]) || queue_time_up ())
    {
      /* Reset time_left before advancing to the next queue */
      mlq_ready_queue[curr_prio].time_left = MAX_PRIO - curr_prio;
      curr_prio++;

      /* Reseting curr_prio when curr_prio reach the end */
      if (curr_prio >= MAX_PRIO)
        {
          curr_prio = 0;
        }
    }
  proc = dequeue (&mlq_ready_queue[curr_prio]);
  pthread_mutex_unlock (&queue_lock);
  return proc;
}

void
put_mlq_proc (struct pcb_t *proc)
{
  pthread_mutex_lock (&queue_lock);
  enqueue (&mlq_ready_queue[proc->prio], proc);
  pthread_mutex_unlock (&queue_lock);
}

void
add_mlq_proc (struct pcb_t *proc)
{
  pthread_mutex_lock (&queue_lock);
  enqueue (&mlq_ready_queue[proc->prio], proc);
  pthread_mutex_unlock (&queue_lock);
}

struct pcb_t *
get_proc (void)
{
  return get_mlq_proc ();
}

void
put_proc (struct pcb_t *proc)
{
  return put_mlq_proc (proc);
}

void
add_proc (struct pcb_t *proc)
{
  return add_mlq_proc (proc);
}

#else

/* Deprecated */
struct pcb_t *
get_proc (void)
{
  struct pcb_t *proc = NULL;
  return proc;
}

void
put_proc (struct pcb_t *proc)
{
  pthread_mutex_lock (&queue_lock);
  enqueue (&run_queue, proc);
  pthread_mutex_unlock (&queue_lock);
}

void
add_proc (struct pcb_t *proc)
{
  pthread_mutex_lock (&queue_lock);
  enqueue (&ready_queue, proc);
  pthread_mutex_unlock (&queue_lock);
}

#ifdef MLQ_SCHED
void
decrease_q_time_left ()
{
  pthread_mutex_lock (&queue_lock);
  mlq_ready_queue[curr_slot].time_left--;
  pthread_mutex_unlock (&queue_lock);
}
#endif

#endif
