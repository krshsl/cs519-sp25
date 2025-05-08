#! /bin/bash
export p31=./linux-5.15.0
export patch_dirs=(arch/x86/entry/syscalls/syscall_64.tbl include/linux/syscalls.h kernel/sched/core.c kernel/fork.c init/init_task.c include/linux/sched.h)
export out_dirs=(syscall_64.patch syscall_h.patch sched_core_c.patch fork_c.patch init_task_c.patch sched_h.patch)
