/*
 * sched.h - low-level scheduler routines (e.g. adding or preempting cores)
 */

#pragma once

#include <base/stddef.h>
#include <base/bitmap.h>
#include <base/limits.h>

#include "defs.h"

struct sched_ops {
	/**
	 * proc_attach - attaches a new process to the scheduler
	 * @p: the new process to attach
	 * @cfg: the requested scheduler configuration of the process
	 *
	 * Typically this is an opportunity to allocate scheduler-specific
	 * data for the process and to validate if the provisioned resources
	 * are available.
	 */
	int (*proc_attach)(struct proc *p, struct sched_spec *cfg);

	/**
	 * proc_detach - detaches an existing process from the scheduler
	 * @p: the existing process to detach
	 *
	 * Typically this is an opportunity to free scheduler-specific
	 * data for the process.
	 */
	void (*proc_detach)(struct proc *p);

	/**
	 * notify_congested - notifies the scheduler of process congestion
	 * @p: the process for which congestion has changed
	 * @threads: a bitmap of congested uthreads (a bit per kthread)
	 * @io: a bitmap of congested I/Os (a bit per kthread)
	 *
	 * This notifier informs the scheduler of when processes become
	 * congested or uncongested, driving core allocation decisions.
	 */
	void (*notify_congested)(struct proc *p, bitmap_ptr_t threads,
			         bitmap_ptr_t io);

	/**
	 * notify_core_needed - notifies the scheduler that a core is needed
	 * @p: the process that needs an additional core
	 *
	 * Returns 0 if a core was added successfully.
	 */
	int (*notify_core_needed)(struct proc *p);

	/**
	 * sched_poll - called each poll loop
	 * @idle: an edge-triggered bitmap of the CPU cores that have become
	 * idle
	 *
	 * Happens right after all notifications. In general, the scheduler
	 * should make adjustments and allocate idle cores during this phase.
	 */
	void (*sched_poll)(bitmap_ptr_t idle);
};


/*
 * Global variables
 */

DECLARE_BITMAP(sched_allowed_cores, NCPU);
unsigned int sched_siblings[NCPU];
unsigned int sched_dp_core;
unsigned int sched_ctrl_core;
unsigned int sched_linux_core;


/*
 * API for scheduler policy modules
 */

extern int sched_run_on_core(struct proc *p, unsigned int core);

static inline int sched_threads_active(struct proc *p)
{
	return p->active_thread_count;
}

static inline int sched_threads_avail(struct proc *p)
{
	return p->thread_count - p->active_thread_count;
}


/*
 * API for the rest of the IOkernel
 */

extern void sched_poll(void);
extern int sched_add_core(struct proc *p);
extern int sched_attach_proc(struct proc *p);
extern void sched_detach_proc(struct proc *p);
