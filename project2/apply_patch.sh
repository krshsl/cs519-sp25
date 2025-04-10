#! /bin/bash
p21=../project2/linux-5.15.0
patch_dirs=(arch/x86/entry/syscalls/syscall_64.tbl include/linux/syscalls.h include/linux/mm.h mm/memory.c mm/Makefile)
for i in ${!patch_dirs[@]}
do
    pF="${p21}/${patch_dirs[i]}"
    echo "${pF}"
    if [ -f "${pF}.orig" ]; then
        cp "${pF}" "${pF}.orig"
    fi
    cp "${pF}.orig" "${pF}.orig1"
done

for pfile in p2_patch/*; do
    if [ -f "$pfile" ]; then
        echo "$pfile"
        git apply --stat $pfile
        git apply --check $pfile
        git apply $pfile
    fi
done

for i in ${!patch_dirs[@]}
do
    pF="${p21}/${patch_dirs[i]}"
    mv "${pF}.orig1" "${pF}.orig"
done
