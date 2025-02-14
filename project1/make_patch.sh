patch_file=project1.patch
if [ -f "$patch_file" ]; then
    rm $patch_file
    touch $patch_file
fi

# can add a new code to look through all the files in the project1 directory and create a patch file for those with .orig extension
patch_dirs="project1/linux-5.15.0/arch/x86/entry/syscalls/syscall_64.tbl,project1/linux-5.15.0/include/linux/syscalls.h,project1/linux-5.15.0/mm/mmap.c"
for i in ${patch_dirs//,/ }
do
    echo "$i"
    diff -u ../$i.orig ../$i >> $patch_file 
done

echo "Updated patch file: $patch_file"