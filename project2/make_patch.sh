#! /bin/bash
patch_file=project2.patch
if [ -f "$patch_file" ]; then
    rm $patch_file
    touch $patch_file
fi

outdir=p2_patch
if [ -d "$outdir" ]; then
    rm -rf $outdir
    mkdir $outdir
fi

p21=../project2/linux-5.15.0
patch_dirs=(arch/x86/entry/syscalls/syscall_64.tbl include/linux/syscalls.h include/linux/mm.h mm/memory.c mm/Makefile)
out_dirs=(syscall_64.patch syscall_h.patch mm_h.patch memory_c.patch mm_makefile.patch)
for i in ${!patch_dirs[@]}
do
    pF="${p21}/${patch_dirs[i]}"
    oF="${outdir}/${out_dirs[i]}"
    diff -u "${pF}.orig" "${pF}" > $oF
    echo "Added patch file: ${oF}"
done

patch_dirs=(mm/my_extent.c include/linux/my_extent.h)
out_dirs=(my_extent_c.patch my_extent_h.patch)
for i in ${!patch_dirs[@]}
do
    pF="${p21}/${patch_dirs[i]}"
    oF="${outdir}/${out_dirs[i]}"
    diff -Nu /dev/null ${pF} > $oF
    echo "Added patch file: ${oF}"
done
