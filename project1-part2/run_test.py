#!/usr/bin/env python3
import subprocess
import pandas as pd
import re
import os
import time
import seaborn as sns
import matplotlib.pyplot as plt

def compile_and_run(method, matrix_size, cores, runs_per_test):
    """Compile and run a single test configuration."""
    results = []
    source_file = f"IPC-{method}.c"
    
    if (matrix_size >= 4096 and cores <= 16):
        print(f"Skipping {method} with matrix size {matrix_size} and cores {cores}")
        return results
    
    # Update source file
    with open(source_file, 'r') as f:
        content = f.read()
    
    content = re.sub(r'#define\s+MATRIX_SIZE\s+\d+', f'#define MATRIX_SIZE {matrix_size}', content)
    content = re.sub(r'#define\s+MAX_CORES\s+\d+', f'#define MAX_CORES {cores}', content)
    
    with open(source_file, 'w') as f:
        f.write(content)
    
    # Compile
    print(f"Compiling {method} with matrix size {matrix_size} and cores {cores}")
    compile_result = subprocess.run(["make"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    if compile_result.returncode != 0:
        print(f"Compilation error: {compile_result.stderr.decode()}")
        return results
    
    # Run multiple times
    for run in range(runs_per_test):
        binary = f"./{method}"
        print(f"Run {run+1}/{runs_per_test}: Executing {binary}")
        
        cmd = ["/usr/bin/time", "-f", "%e,%U,%S", "-q", binary, "-p"]
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = process.communicate()
        
        try:
            program_output = stdout.decode().strip()
            time_taken = float(program_output.split(',')[-1])
            
            time_metrics = stderr.decode().strip().split(',')
            real_time, user_time, sys_time = map(float, time_metrics)
            
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
            
        except Exception as e:
            print(f"Error processing output: {e}")
        
        time.sleep(1)
    
    return results

def save_results(df):
    """Save raw results and generate summary statistics."""
    df.to_csv('ipc_benchmark_results.csv', index=False)
    
    summary = df.groupby(['Method', 'MatrixSize', 'Cores']).agg({
        'TimeTaken': ['mean', 'std', 'min', 'max'],
        'RealTime': ['mean', 'std'],
        'UserTime': ['mean', 'std'],
        'SysTime': ['mean', 'std']
    })
    
    summary.to_csv('ipc_benchmark_summary.csv')
    print("Testing completed. Results saved to ipc_benchmark_results.csv and ipc_benchmark_summary.csv")

def generate_plots(df):
    """Generate performance plots for each method."""
    sns.set(style="darkgrid")
    
    for method in df['Method'].unique():
        plt.figure(figsize=(12, 8))
        
        method_data = df[df['Method'] == method]
        
        sns.lineplot(
            data=method_data,
            x='Cores',
            y='TimeTaken',
            hue='MatrixSize',
            marker='o',
            palette='viridis',
        )
        
        plt.title(f'Performance of {method.upper()} method by core count', fontsize=16)
        plt.xlabel('Number of Cores', fontsize=14)
        plt.ylabel('Time Taken (seconds)', fontsize=14)
        plt.legend(title='Matrix Size', fontsize=12)
        plt.grid(True)
        plt.tight_layout()
        
        plt.savefig(f'{method}_performance.png')
        print(f"Created graph for {method} method (saved as {method}_performance.png)")
    
    plt.close('all')

def generate_cpu_utilization_plots(df):
    """Generate CPU utilization plots for each method."""
    sns.set(style="darkgrid")
    
    # Calculate CPU utilization
    df['CPUUtilization'] = (df['UserTime'] + df['SysTime'] + df['TimeTaken'] - df['RealTime']) / (df['TimeTaken'] * df['Cores']) * 100
    
    for method in df['Method'].unique():
        plt.figure(figsize=(12, 8))
        
        method_data = df[df['Method'] == method]
        
        sns.lineplot(
            data=method_data,
            x='Cores',
            y='CPUUtilization',
            hue='MatrixSize',
            marker='o',
            palette='viridis',
        )
        
        plt.title(f'CPU Utilization of {method.upper()} method by core count', fontsize=16)
        plt.xlabel('Number of Cores', fontsize=14)
        plt.ylabel('CPU Utilization (%)', fontsize=14)
        plt.legend(title='Matrix Size', fontsize=12)
        plt.grid(True)
        plt.tight_layout()
        
        plt.savefig(f'{method}_cpu_utilization.png')
        print(f"Created CPU utilization graph for {method} method (saved as {method}_cpu_utilization.png)")
    
    plt.close('all')

def generate_perf_plots():
    """Generate performance plots from existing results."""    
    # Read from existing results file instead of running tests
    try:
        print("Reading results from ipc_benchmark_results.csv")
        df = pd.read_csv('ipc_benchmark_results.csv')
        print(f"Loaded {len(df)} test results")
    except FileNotFoundError:
        print("Results file not found. Please run the tests first.")
        return

    # Generate performance plots
    generate_plots(df)

    # Generate cpu utilization plots
    generate_cpu_utilization_plots(df)

def main():
    """Main function to execute the benchmark tests."""
    # Parameters
    matrix_sizes = [32, 128, 512, 1024, 2048, 4096, 8192, 10000]
    core_counts = [2, 4, 8, 16, 32, 48, 64, 96]
    methods = ["pipe", "shmem"]
    runs_per_test = 50
    
    # Run tests and collect results
    all_results = []
    
    for method in methods:
        for matrix_size in matrix_sizes:
            for cores in core_counts:
                results = compile_and_run(method, matrix_size, cores, runs_per_test)
                all_results.extend(results)
    
    # Create DataFrame from results
    df = pd.DataFrame(all_results)
    
    # Save results and generate summary
    save_results(df)
    
    # Generate performance plots
    generate_perf_plots()

if __name__ == "__main__":
    main()