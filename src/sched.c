/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "sched.h"
#include "queue.h"
#include "common.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static struct queue_t running_list;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

static pthread_mutex_t queue_lock;

int queue_empty(void) {
#ifdef MLQ_SCHED
	for (int prio = 0; prio < MAX_PRIO; prio++) {
		if (!empty(&mlq_ready_queue[prio]))
			return 0;
	}
#endif

	return empty(&ready_queue) && empty(&run_queue);
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
	for (int i = 0; i < MAX_PRIO; i++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i;
	}
#endif

	ready_queue.size = 0;
	run_queue.size = 0;
	running_list.size = 0;

	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
struct pcb_t *get_mlq_proc(void) {
	struct pcb_t *proc = NULL;

	pthread_mutex_lock(&queue_lock);

	for (int prio = 0; prio < MAX_PRIO; prio++) {
		if (!empty(&mlq_ready_queue[prio]) && slot[prio] > 0) {
			proc = dequeue(&mlq_ready_queue[prio]);
			slot[prio]--;
			break;
		}
	}

	if (proc == NULL) {
		int has_proc = 0;

		for (int prio = 0; prio < MAX_PRIO; prio++) {
			if (!empty(&mlq_ready_queue[prio])) {
				has_proc = 1;
				break;
			}
		}

		if (has_proc) {
			for (int prio = 0; prio < MAX_PRIO; prio++)
				slot[prio] = MAX_PRIO - prio;

			for (int prio = 0; prio < MAX_PRIO; prio++) {
				if (!empty(&mlq_ready_queue[prio]) && slot[prio] > 0) {
					proc = dequeue(&mlq_ready_queue[prio]);
					slot[prio]--;
					break;
				}
			}
		}
	}

	if (proc != NULL)
		enqueue(&running_list, proc);

	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_mlq_proc(struct pcb_t *proc) {
	if (proc == NULL)
		return;

	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	pthread_mutex_lock(&queue_lock);

	purgequeue(&running_list, proc);

	if (proc->prio >= MAX_PRIO)
		proc->prio = MAX_PRIO - 1;

	enqueue(&mlq_ready_queue[proc->prio], proc);

	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc) {
	if (proc == NULL)
		return;

	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	pthread_mutex_lock(&queue_lock);

	if (proc->prio >= MAX_PRIO)
		proc->prio = MAX_PRIO - 1;

	enqueue(&mlq_ready_queue[proc->prio], proc);

	pthread_mutex_unlock(&queue_lock);
}
#endif

struct pcb_t *get_proc(void) {
	struct pcb_t *proc = NULL;

#ifdef MLQ_SCHED
	proc = get_mlq_proc();
#else
	pthread_mutex_lock(&queue_lock);

	if (!empty(&ready_queue))
		proc = dequeue(&ready_queue);
	else if (!empty(&run_queue))
		proc = dequeue(&run_queue);

	if (proc != NULL)
		enqueue(&running_list, proc);

	pthread_mutex_unlock(&queue_lock);
#endif

	return proc;
}

void put_proc(struct pcb_t *proc) {
	if (proc == NULL)
		return;

#ifdef MLQ_SCHED
	put_mlq_proc(proc);
#else
	pthread_mutex_lock(&queue_lock);

	purgequeue(&running_list, proc);
	enqueue(&run_queue, proc);

	pthread_mutex_unlock(&queue_lock);
#endif
}

void add_proc(struct pcb_t *proc) {
	if (proc == NULL)
		return;

	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

#ifdef MLQ_SCHED
	add_mlq_proc(proc);
#else
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);
#endif
}
