
**CAUTION:** Copying code from online sources or automatic generation tools is
not acceptable and will be reported.

System calls serve as one of the most fundamental interfaces between user
processes and the operating system’s kernel. Whenever a user application needs
to request a hardware resource or perform certain privileged operations, it
invokes a system call. 

In this project, you will explore the mechanics and
overhead associated with system calls by creating and measuring the performance
of a custom (dummy) system call within the **Linux 5.15** kernel, specifically
modifying the
[mm/mmap.c](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/mm/mmap.c?h=linux-5.15.y)
file. Through this exercise, you will gain a deeper understanding of the data
transfer boundary between user space and kernel space and how even seemingly
trivial operations can impact system performance.

---

## 0. Compiling the Linux Kernel

First, download the Linux kernel source code. The `uname -r` command retrieves the current Linux version:

```
sudo apt-get update
sudo sed -i 's/# deb-src/deb-src/' /etc/apt/sources.list

//Do not use sudo for this step as it just downloads the source files
apt source linux-image-unsigned-$(uname -r)
```

Then, use our scripts to compile the kernel. The first script is a slower
compilation process that can take 30–40 minutes; fortunately, this slower step
only needs to be performed once per node.

First, navigate to the Linux source tree:

```
cd linux-5.15.0
```

Then run the installation script. 

**Note, this script can be run only for the first time installation of the kernel in a CloudLab node.** 
```
../install_packages.sh
```

This is the main script to compile. The overall compilation process can take more than 30 to 40 minutes. 
```
../compile_os_nopackages.sh
```

For all subsequent compilation of kernels, you can just use the fast
compilation script after making the change.
```
../compile_os_quick.sh
```

You will see that the kernel has been installed. Most likely, it would be version 5.15.168. 

Now, time to reboot
```
sudo reboot
```


## 1. Adding a Simple System Call
You will begin by creating a system call, tentatively called **`app_helper`**,
in the kernel source file **mm/mmap.c**. The primary function of this new system
call is to accept a pointer to a user-space buffer (which you will allocate
using `malloc` in a user application) and the size of that buffer. Once the call
is invoked, the kernel must then copy the data from this user-level buffer into
a kernel buffer. Inside the kernel, you will modify that data (for example,
setting all bytes to **`1`**) before copying the updated contents back to user
space.

To confirm that your system call is being invoked, you should insert a
**`printk()`** statement in the new function’s body. Check the kernel log (e.g.,
using `dmesg`) to see the message printed. Once you have verified that the
system call is triggered, remove or comment out the `printk()` statement to
ensure accurate performance measurements—logging can skew timing results
significantly.

For practical guidance on how to integrate a new system call, refer to the following online resources:
- [Main Reference](https://linux-kernel-labs.github.io/refs/pull/183/merge/lectures/syscalls.html) 
- [Reference 1](https://shanetully.com/2014/04/adding-a-syscall-to-linux-3-14/)  
- [Reference 2](https://macboypro.wordpress.com/2009/05/15/adding-a-custom-system-call-to-the-linux-os/)  

(You can also find many additional resources online if you need further clarification.)

---

## 2. Measuring the Cost of the System Call

After successfully adding the **`app_helper`** system call, your next step is to measure how long it takes to invoke it from a user program. Write a **benchmarking** user application that does the following:

1. **Allocates a buffer in user space** (using `malloc`) and initializes it (e.g., using `memset(buf, 4, size)`).
2. **Invokes the `app_helper` system call** multiple times (for example, a few hundred or thousand times).
3. **Measures the average latency** per call. Use timing functions (e.g., `gettimeofday()` or `clock_gettime(CLOCK_MONOTONIC, ...)`) to capture the total elapsed time for all invocations, then compute the average cost for a single call.

Make sure you also **validate** the results by checking that your buffer has indeed been modified to **`1`** when the system call returns. This validation confirms that your kernel copy-back code (e.g., `copy_to_user`) is functioning correctly.

**PLEASE NOTE: We will use our own benchmark code to invoke your `app_helper`
system call and time it when testing.

---

## 3. Experiment Details

1. **System Call Buffer Transfer**  
   - **From user to kernel**: Use safe copy functions like `copy_from_user()` to move data from the user buffer into a temporary kernel buffer.  
   - **In the kernel**: Change the data to some known value (`1`).  
   - **From kernel back to user**: Use `copy_to_user()` to write the modified data back to the user buffer.
   - You can find more details here [copying to and from OS](https://linux-kernel-labs.github.io/refs/pull/183/merge/lectures/syscalls.html?highlight=copy_from_user)

2. **Buffer Allocation and Initialization**  
   - Allocate the user-space buffer with `malloc()`.  
   - Initialize it with value **`4`** (using something like `memset(buf, 4, size)`) before calling your system call.

3. **Verification**  
   - In your user application, write a small function that checks whether all bytes in the user buffer are now **`1`** after the system call returns. If they are not, log an error or print a diagnostic message.

4. **Timing**  
   - To reduce noise in your measurements, **remove** any unnecessary logging in the kernel and user space once you confirm correctness.  
   - Consider running several trials to ensure repeatability and to account for any fluctuations in system load.

---

## 4. Deliverables and Reporting

Your final submission should include:

1. **Kernel Patch or Modified Code**  
   - Show the relevant changes in all files including **mm/mmap.c** (and wherever you add the system call) that implement `app_helper`. 
   - To generate the patch for each file:
Use the diff command to generate a unified diff for each modified file. For example, for mm/mmap.c:
```
diff -u path/to/mm/mmap.c.orig path/to/mm/mmap.c > mm/mmap.c.patch
```

2. **User-Space Benchmark Program**  
   - A C program that:
     - Allocates and initializes a buffer with `4`.  
     - Invokes the `app_helper` system call multiple times in a loop.  
     - Measures and prints the average latency per call.  
     - Validates that the buffer is updated to `1`.

3. **Results and Analysis**  
   - A brief write-up reporting the average latency of the system call over multiple iterations.  
   - Any notable findings, such as how many microseconds each call takes or variability observed.

---


#### Computing Resource
Please use CloudLab for this assignment.


#### Starting Early
This an essential homework for understanding the basics. 
Please start working on this homework early. If you have
questions, make sure to ask them during office hours.

