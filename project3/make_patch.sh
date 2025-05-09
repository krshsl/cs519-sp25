#! /bin/bash
outdir=p3_patch
if [ -d $outdir ]; then
    rm -rf $outdir
fi
mkdir $outdir

. ./set_patch_env.sh
for i in ${!patch_dirs[@]}
do
    pF="${p31}/${patch_dirs[i]}"
    oF="${outdir}/${out_dirs[i]}"
    diff -u "${pF}.orig" "${pF}" > $oF
    echo "Added patch file: ${oF}"
done

patch_dirs=(include/linux/my_inactive.h)
out_dirs=(my_inactive_h.patch)
for i in ${!patch_dirs[@]}
do
    pF="${p31}/${patch_dirs[i]}"
    oF="${outdir}/${out_dirs[i]}"
    diff -Nu /dev/null ${pF} > $oF
    echo "Added patch file: ${oF}"
done
