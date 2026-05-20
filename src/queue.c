#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
    if ( q->size + 1 <= MAX_QUEUE_SIZE ) {
        q->proc[q->size] = proc;
        q->size++;
    }
    else {
        printf("MAX_QUEUE_SIZE exceed inside enqueue()");
    }
}

struct pcb_t *dequeue(struct queue_t *q)
{
    /* TODO: return a pcb whose prioprity is the highest
     * in the queue [q] and remember to remove it from q
     */

    /* This is fifo so naturally, just rmove the first element
       and let the sched take care of the logic. */

    if ( q->size == 0 ) { return NULL; }

    struct pcb_t *deque_proc = q->proc[0];
    int i;
    for (i = 0; i < q->size - 1; i++) {
        q->proc[i] = q->proc[i + 1];
    }
    q->size--;

    return deque_proc;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
        return NULL;
}
