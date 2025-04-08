patch_file=project2.patch
if [ -f "$patch_file" ]; then
    rm $patch_file
    touch $patch_file
fi

p2l=project2/linux-5.15.0
patch_dirs="${p2l}/arch/x86/entry/syscalls/syscall_64.tbl,${p2l}/include/linux/syscalls.h,${p2l}/mm/memory.c,${p2l}/include/linux/syscalls.h,${p2l}/mm/Makefile"
for i in ${patch_dirs//,/ }
do
    echo "$i"
    diff -u ../$i.orig ../$i >> $patch_file 
done

diff -Nu /dev/null ../${p2l}/mm/my_extent.c >> $patch_file 

echo "Updated patch file: $patch_file"