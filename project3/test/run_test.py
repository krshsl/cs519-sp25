#!/usr/bin/env python3
import subprocess
import pandas as pd
import re
import os
import time
import seaborn as sns
import matplotlib.pyplot as plt

func_modes = {
    "shmem": {
        "1": "shmem no change",
        "2": "shmem syscall added",
        "3": "shmem with yield"
    }
}

def compile_and_run(method, modes, matrix_size, cores, runs_per_test):
    """Compile and run a single test configuration."""
    results = []
    source_file = f"IPC-{method}.c"

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
    for mode in modes:
        for run in range(runs_per_test):
            binary = f"./{method}_{mode}.o"
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
                    'Method': func_modes[method][mode],
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
    # Check if file exists to append or create new
    if os.path.exists('ipc_benchmark_results.csv'):
        # Append without writing header
        df.to_csv('ipc_benchmark_results.csv', mode='a', header=False, index=False)
        print("Appended results to existing file ipc_benchmark_results.csv")
    else:
        # Create new file with header
        df.to_csv('ipc_benchmark_results.csv', index=False)
        print("Created new results file ipc_benchmark_results.csv")

    summary = df.groupby(['Method', 'MatrixSize', 'Cores']).agg({
        'TimeTaken': ['mean', 'std', 'min', 'max'],
        'RealTime': ['mean', 'std'],
        'UserTime': ['mean', 'std'],
        'SysTime': ['mean', 'std']
    })

    summary.to_csv('ipc_benchmark_summary.csv')
    print("Testing completed. Results saved to ipc_benchmark_results.csv and ipc_benchmark_summary.csv")

def generate_plots(df):
    """Generate performance plots comparing all methods on the same graph."""
    sns.set(style="darkgrid")
    os.makedirs("images", exist_ok=True)

    # Plot all methods together on the same graph
    # Small matrices (above 1024)
    small_data = df[df['MatrixSize'] > 1024]
    if not small_data.empty:
        plt.figure(figsize=(12, 8))

        # Create a new column that combines Method and MatrixSize for the legend
        small_data['Method_Size'] = small_data.apply(
            lambda row: f"{row['Method']} - {row['MatrixSize']}", axis=1
        )

        sns.lineplot(
            data=small_data,
            x='Cores',
            y='TimeTaken',
            hue='Method_Size',
            style='Method',
            markers=True,
            dashes=False,
            palette='viridis',
        )

        plt.title('Performance Comparison', fontsize=16)
        plt.xlabel('Number of Cores', fontsize=14)
        plt.ylabel('Time Taken (seconds)', fontsize=14)
        plt.legend(fontsize=12, loc='center left', bbox_to_anchor=(1, 0.5))
        plt.grid(True)
        plt.tight_layout()

        plt.savefig('images/matrix_performance.png', bbox_inches='tight')
        print("Created performance graph comparing all methods with small matrices")

    plt.close('all')

def generate_cpu_utilization_plots(df):
    """Generate CPU utilization plots for each method with separate graphs for small and large matrices."""
    sns.set(style="darkgrid")

    # Calculate CPU utilization
    df['CPUUtilization'] = (df['UserTime'] + df['SysTime'] + df['TimeTaken'] - df['RealTime']) / (df['TimeTaken'] * df['Cores']) * 100

    # Plot all methods together on the same graph
    # Small matrices (above 1024)
    small_data = df[df['MatrixSize'] > 1024]
    if not small_data.empty:
        plt.figure(figsize=(12, 8))

        # Create a new column that combines Method and MatrixSize for the legend
        small_data['Method_Size'] = small_data.apply(
            lambda row: f"{row['Method']} - {row['MatrixSize']}", axis=1
        )

        sns.lineplot(
            data=small_data,
            x='Cores',
            y='CPUUtilization',
            hue='Method_Size',
            style='Method',
            markers=True,
            dashes=False,
            palette='viridis',
        )

        plt.title(f'CPU Utilization Comparison', fontsize=16)
        plt.xlabel('Number of Cores', fontsize=14)
        plt.ylabel('CPU Utilization (%)', fontsize=14)
        plt.legend(fontsize=12, loc='center left', bbox_to_anchor=(1, 0.5))
        plt.grid(True)
        plt.tight_layout()

        plt.savefig(f'images/matrix_cpu_utilization.png', bbox_inches='tight')
        print(f"Created CPU utilization graph comparing all methods with small matrices")

    plt.close('all')

def generate_additional_plots(df, summary_df, matrix, key):
    """Generate additional plots for better analysis of matrices."""
    matrix_df = df[df['MatrixSize'].isin(matrix)]
    matrix_df = matrix_df[matrix_df['Cores'] <= 40]

    plt.figure(figsize=(12, 8))
    for method in matrix_df['Method'].unique():
        for size in matrix_df[matrix_df['Method'] == method]['MatrixSize'].unique():
            method_size_data = matrix_df[(matrix_df['Method'] == method) &
                                                (matrix_df['MatrixSize'] == size)]
            if method_size_data.empty:
                continue

            # Get average time for minimum core count
            min_cores = method_size_data['Cores'].min()
            base_time = method_size_data[method_size_data['Cores'] == min_cores]['TimeTaken'].mean()

            # Calculate speedup for each core count
            cores = sorted(method_size_data['Cores'].unique())
            speedups = []
            for core in cores:
                avg_time = method_size_data[method_size_data['Cores'] == core]['TimeTaken'].mean()
                speedups.append(base_time / avg_time)

            plt.plot(cores, speedups, marker='o', label=f"{method.upper()} - {size}")

    max_cores = matrix_df['Cores'].max()
    min_cores = matrix_df['Cores'].min()
    if min_cores > 0:
        ideal = [c/min_cores for c in range(min_cores, max_cores+1)]
        plt.plot(range(min_cores, max_cores+1), ideal, 'k--', label='Ideal Linear Speedup')

    plt.title(f'Speedup vs. Number of Cores for {key} Matrices', fontsize=16)
    plt.xlabel('Number of Cores', fontsize=14)
    plt.ylabel('Speedup (relative to minimum core count)', fontsize=14)
    plt.grid(True)
    plt.legend(fontsize=12)
    plt.tight_layout()
    plt.savefig(f'images/{key}_matrices_speedup.png', bbox_inches='tight')
    print(f"Created speedup analysis plot for {key} matrices")

    matrix_df = df[df['MatrixSize'].isin(matrix)]
    for size in matrix:
        size_data = matrix_df[matrix_df['MatrixSize'] == size]
        if size_data.empty:
            continue

        # Create pivot table for heatmap
        heatmap_data = size_data.pivot_table(
            values='TimeTaken',
            index='Method',
            columns='Cores',
            aggfunc='mean'
        )

        plt.figure(figsize=(14, 6))
        sns.heatmap(heatmap_data, annot=True, fmt='.5f', cmap='viridis', cbar_kws={'label': 'Time (seconds)'})
        plt.title(f'Mean Execution Time Heatmap for Matrix Size {size}', fontsize=16)
        plt.xlabel('Number of Cores', fontsize=14)
        plt.ylabel('Method', fontsize=14)
        plt.tight_layout()
        plt.savefig(f'images/matrix_size_{size}_heatmap.png', bbox_inches='tight')
        print(f"Created heatmap for matrix size {size}")

    plt.close('all')

def generate_perf_plots():
    """Generate performance plots from existing results."""
    # Read from existing results file instead of running tests
    try:
        print("Reading results from ipc_benchmark_results.csv")
        df = pd.read_csv('ipc_benchmark_results.csv')
        summary_df = pd.read_csv('ipc_benchmark_summary.csv')
        print(f"Loaded {len(df)} test results")
        print(f"Loaded {len(summary_df)} summary results")
    except FileNotFoundError:
        print("Results file not found. Please run the tests first.")
        return
    os.makedirs("images", exist_ok=True)

    # Generate performance plots
    generate_plots(df)

    # Generate cpu utilization plots
    generate_cpu_utilization_plots(df)

    generate_additional_plots(df, summary_df, [4096, 5000], "large")

def main():
    """Main function to execute the benchmark tests."""
    # Parameters
    matrix_sizes = [4096, 5000]
    core_counts = [16, 32, 48, 64]
    runs_per_test = 10

    # Run tests and collect results
    all_results = []

    for method in list(func_modes.keys()):
        for matrix_size in matrix_sizes:
            for cores in core_counts:
                results = compile_and_run(method, list(func_modes[method].keys()), matrix_size, cores, runs_per_test)
                all_results.extend(results)

    # Create DataFrame from results
    df = pd.DataFrame(all_results)

    # Save results and generate summary
    save_results(df)

    # Generate performance plots
    generate_perf_plots()

if __name__ == "__main__":
    main()
