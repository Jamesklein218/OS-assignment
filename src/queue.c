#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void
resize (struct queue_t *q, int new_size)
{
  /* If the input is invalid, does nothing */
  if (q == NULL || new_size < 0)
    return;
  /* Reallocate enough memory for the new_size if neccessary */
  if (new_size > (q->size * 2 / 3))
    {
      int new_cap = (new_size > q->size ? new_size : q->size)
                    * 2;                        /* Generate new capacity */
      struct pcb_t **new_proc = (struct pcb_t **)malloc (
          new_cap * (sizeof (struct pcb_t *))); /* Allocating new memory */

      /* Copying old value to the new memory space */
      for (int i = 0; i < q->size; i++)
        new_proc[i] = q->proc[i];

      /* Free the old memory */
      if (q->proc)
        free (q->proc);

      /* Updating queue value */
      q->proc = new_proc;
      q->capacity = new_cap;
      q->size = new_size;
    }
  /* Otherwise, only change the size of the queue */
  else
    {
      q->size = new_size;
    }
}

int
empty (struct queue_t *q)
{
  if (q == NULL || q->size == 0)
    return 1;
  return (q->size == 0);
}

void
enqueue (struct queue_t *q, struct pcb_t *proc)
{
  resize (q, q->size + 1);
  q->proc[q->size - 1] = proc;
}

struct pcb_t *
dequeue (struct queue_t *q)
{
  /* Check if the queue is empty */
  if (empty (q))
    return NULL;

  struct pcb_t *res = q->proc[0];

  /* Shifting all element */
  for (int i = 0; i < q->size - 1; i++)
    q->proc[i] = q->proc[i + 1];
  resize (q, q->size - 1);

  return res;
}
