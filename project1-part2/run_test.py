#!/usr/bin/env python3
import subprocess
import pandas as pd
import re
import os
import time

# Parameters
matrix_sizes = [32, 128, 512]
core_counts = [2, 5, 10, 20, 30, 40]
methods = ["pipe", "shmem"]
runs_per_test = 10

# Create results DataFrame
results = []

for method in methods:
    for matrix_size in matrix_sizes:
        for cores in core_counts:
            # Modify source file based on method
            source_file = f"IPC-{method}.c"

            if (method == "shmem" and matrix_size == 10000):
                print(f"Skipping shmem with matrix size {matrix_size} as it is too large")
                continue
                
            
            # Read current file content
            with open(source_file, 'r') as f:
                content = f.read()
            
            # Update matrix size and max_cores
            content = re.sub(r'#define\s+MATRIX_SIZE\s+\d+', f'#define MATRIX_SIZE {matrix_size}', content)
            content = re.sub(r'#define\s+MAX_CORES\s+\d+', f'#define MAX_CORES {cores}', content)
            
            # Write updated content
            with open(source_file, 'w') as f:
                f.write(content)
            
            # Compile
            print(f"Compiling {method} with matrix size {matrix_size} and cores {cores}")
            compile_result = subprocess.run(["make"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            
            if compile_result.returncode != 0:
                print(f"Compilation error: {compile_result.stderr.decode()}")
                continue
                
            # Run multiple times
            for run in range(runs_per_test):
                binary = f"./{method}"
                print(f"Run {run+1}/{runs_per_test}: Executing {binary}")
                
                # Execute with time command
                cmd = ["/usr/bin/time", "-f", "%e,%U,%S", "-q", binary, "-p"]
                process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                stdout, stderr = process.communicate()
                
                # Parse output
                try:
                    # Get program output for time taken
                    program_output = stdout.decode().strip()
                    time_taken = float(program_output.split(',')[-1])  # Assuming program outputs time taken
                    
                    # Parse time command output
                    time_metrics = stderr.decode().strip().split(',')
                    real_time, user_time, sys_time = map(float, time_metrics)
                    
                    # Store result
                    results.append({
                        'Method': method,
                        'MatrixSize': matrix_size,
                        'Cores': cores,
                        'Run': run+1,
                        'TimeTaken': time_taken,
                        'RealTime': real_time,
                        'UserTime': user_time,
                        'SysTime': sys_time
                    })
                    
                    print(f"Result: {matrix_size},{cores},{method},{time_taken}")
                    print(f"Time: {real_time},{user_time},{sys_time}")
                except Exception as e:
                    print(f"Error processing output: {e}")
                
                # Wait a bit between runs to allow system to stabilize
                time.sleep(1)

# Create DataFrame and save results
df = pd.DataFrame(results)
df.to_csv('ipc_benchmark_results.csv', index=False)

# Generate summary statistics
summary = df.groupby(['Method', 'MatrixSize', 'Cores']).agg({
    'TimeTaken': ['mean', 'std', 'min', 'max'],
    'RealTime': ['mean', 'std'],
    'UserTime': ['mean', 'std'],
    'SysTime': ['mean', 'std']
})

summary.to_csv('ipc_benchmark_summary.csv')
print("Testing completed. Results saved to ipc_benchmark_results.csv and ipc_benchmark_summary.csv")