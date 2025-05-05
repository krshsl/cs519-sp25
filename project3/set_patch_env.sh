#! /bin/bash
export p31=./linux-5.15.0
export patch_dirs=(arch/x86/entry/syscalls/syscall_64.tbl include/linux/syscalls.h kernel/sched/core.c)
export out_dirs=(syscall_64.patch syscall_h.patch sched_core_c.patch)
