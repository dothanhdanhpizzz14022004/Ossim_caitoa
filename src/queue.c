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
        if (q == NULL || proc == NULL)
                return;

        if (q->size >= MAX_QUEUE_SIZE)
        {
                printf("Queue overflow\n");
                return;
        }

        q->proc[q->size] = proc;
        q->size++;
}
struct pcb_t *dequeue(struct queue_t *q)
{
    /* TODO: return a pcb whose prioprity is the highest
     * in the queue [q] and remember to remove it from q
     */

    /* This is fifo so naturally, just rmove the first element
       and let the sched take care of the logic. */

    if (q == NULL || q->size == 0)
                return NULL;

        struct pcb_t *ret = q->proc[0];

        for (int i = 0; i < q->size - 1; i++)
                q->proc[i] = q->proc[i + 1];

        q->proc[q->size - 1] = NULL;
        q->size--;

        return ret;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
        if (q == NULL || proc == NULL || q->size == 0)
                return NULL;

        int pos = -1;

        for (int i = 0; i < q->size; i++)
        {
                if (q->proc[i] == proc)
                {
                        pos = i;
                        break;
                }
        }

        if (pos == -1)
                return NULL;

        struct pcb_t *ret = q->proc[pos];

        for (int i = pos; i < q->size - 1; i++)
                q->proc[i] = q->proc[i + 1];

        q->proc[q->size - 1] = NULL;
        q->size--;

        return ret;
}
