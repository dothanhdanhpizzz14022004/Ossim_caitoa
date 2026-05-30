/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "queue.h"
#include "sched.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
static int current_prio = 0;
static int remaining_slot = 0;
#endif

int queue_empty(void)
{
#ifdef MLQ_SCHED
	int prio;
	for (prio = 0; prio < MAX_PRIO; prio++) {
		if (!empty(&mlq_ready_queue[prio]))
			return 0;
	}
#endif
	return empty(&ready_queue) && empty(&run_queue);
}

void init_scheduler(void)
{
#ifdef MLQ_SCHED
	int i;
	for (i = 0; i < MAX_PRIO; i++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i;
	}

	current_prio = 0;
	remaining_slot = slot[current_prio];
#endif

	ready_queue.size = 0;
	run_queue.size = 0;
	running_list.size = 0;

	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED

static void advance_prio(void)
{
	current_prio = (current_prio + 1) % MAX_PRIO;
	remaining_slot = slot[current_prio];
}

struct pcb_t *get_mlq_proc(void)
{
	struct pcb_t *proc = NULL;

	pthread_mutex_lock(&queue_lock);

	/*
	 * MLQ policy:
	 * - Each priority queue has a fixed number of slots.
	 * - If the current queue is empty, do not waste all its remaining slots.
	 *   Move immediately to the next priority queue.
	 * - Search at most MAX_PRIO queues to avoid infinite looping.
	 */
	for (int tried = 0; tried < MAX_PRIO; tried++) {
		if (remaining_slot <= 0)
			advance_prio();

		if (empty(&mlq_ready_queue[current_prio])) {
			advance_prio();
			continue;
		}

		proc = dequeue(&mlq_ready_queue[current_prio]);
		remaining_slot--;
		break;
	}

	if (proc != NULL)
		enqueue(&running_list, proc);

	pthread_mutex_unlock(&queue_lock);

	return proc;
}

void put_mlq_proc(struct pcb_t *proc)
{
	if (proc == NULL)
		return;

	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	if (proc->prio >= MAX_PRIO)
		proc->prio = MAX_PRIO - 1;

	pthread_mutex_lock(&queue_lock);

	purgequeue(&running_list, proc);
	enqueue(&mlq_ready_queue[proc->prio], proc);

	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc)
{
	if (proc == NULL)
		return;

	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	if (proc->prio >= MAX_PRIO)
		proc->prio = MAX_PRIO - 1;

	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

struct pcb_t *get_proc(void)
{
	return get_mlq_proc();
}

void put_proc(struct pcb_t *proc)
{
	put_mlq_proc(proc);
}

void add_proc(struct pcb_t *proc)
{
	add_mlq_proc(proc);
}

#else

struct pcb_t *get_proc(void)
{
	struct pcb_t *proc = NULL;

	pthread_mutex_lock(&queue_lock);

	if (!empty(&ready_queue))
		proc = dequeue(&ready_queue);
	else if (!empty(&run_queue))
		proc = dequeue(&run_queue);

	if (proc != NULL)
		enqueue(&running_list, proc);

	pthread_mutex_unlock(&queue_lock);

	return proc;
}

void put_proc(struct pcb_t *proc)
{
	if (proc == NULL)
		return;

	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;

	pthread_mutex_lock(&queue_lock);
	purgequeue(&running_list, proc);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc)
{
	if (proc == NULL)
		return;

	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;

	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}
#endif
