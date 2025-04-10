#! /bin/bash
p21=../project2/linux-5.15.0
patch_dirs=(arch/x86/entry/syscalls/syscall_64.tbl include/linux/syscalls.h include/linux/mm.h mm/memory.c mm/Makefile)
for i in ${!patch_dirs[@]}
do
    pF="${p21}/${patch_dirs[i]}"
    if [ -f "${pF}.orig" ]; then
        echo "Reverting ${pF}"
        mv "${pF}.orig" "${pF}"
    fi
done
