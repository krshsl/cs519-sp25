from re import search
from csv import writer
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
import subprocess
import os

perf_data = "perf.data"
csv_file = "output.csv"
thread_sim = [8, 16, 32, 64]
exec_files = [['./locks_bench', 'global_0'], ['./locks_bench_local', 'local']]
inc_y_index = [1.25, 1.5]
max_repeat = 100

folders = {
    1: "Compare-and-Swap",
    2: "Test-and-Set",
    3: "Ticket-Lock",
    4: "Mutex",
    5: "Semaphore"
}


def get_perf_data(index):
    for mode in range(1, 6):
        for threads in thread_sim:
            for iters in [100, 1000, 10000]:
                if (threads == 64 or threads == 32) and iters == 10000:
                    continue # takes too long...

                for worklen in [10, 100, 1000]:
                    output_dir = os.path.join(exec_files[index][1], folders[mode], str(threads), str(iters), str(worklen))
                    output_file = os.path.join(output_dir, perf_data)
                    if not os.path.isdir(output_dir):
                        os.makedirs(output_dir)
                    
                    result = subprocess.run(['sudo', 'perf', 'record', '-a', '-g', '-o', output_file, exec_files[index][0], str(mode), str(threads), str(iters), str(worklen)], capture_output=True, text=True)
                    result = search(r"(\d+\.\d+) seconds", result.stdout)
                    result = float(result.group(1)) if result else None

def run_sim(csvWriter, index):
    for mode in range(1, 6):
        for threads in thread_sim:
            for iters in [100, 1000, 10000]:
                if (threads == 64 or threads == 32) and iters == 10000:
                    continue # takes too long...

                for worklen in [10, 100, 1000]:
                    curr_avg = 0
                    total = 0
                    for repeat in range(max_repeat):
                        result = subprocess.run([exec_files[index][0], str(mode), str(threads), str(iters), str(worklen)], capture_output=True, text=True)
                        result = search(r"(\d+\.\d+) seconds", result.stdout)
                        result = float(result.group(1)) if result else None
                        if result:
                            total += 1
                            curr_avg += result
                    
                    curr_avg /= total
                    csvWriter.writerow([folders[mode], threads, iters, worklen, result])
            

def collect_data(index):
    if not os.path.isdir(exec_files[index][1]):
        os.makedirs(exec_files[index][1])

    output_file = os.path.join(exec_files[index][1], csv_file)
    with open(output_file, "w") as file:
        csvWriter = writer(file)
        csvWriter.writerow(['Mode', 'Threads', 'Iterations', 'Work Length', 'Time'])
        run_sim(csvWriter, index)

def gen_graphs(index):
    input_file = os.path.join(exec_files[index][1], csv_file)
    df = pd.read_csv(input_file)
    thread_counts = df["Threads"].unique()

    for threads in thread_counts:
        t_mode = df[df["Threads"] == threads]
        iterations_counts = t_mode["Iterations"].unique()
        fig, axes = plt.subplots(len(iterations_counts), 1, figsize=(18, 9), sharex=True)
        
        for i, iterations in enumerate(iterations_counts):
            subset = t_mode[t_mode["Iterations"] == iterations]
            sns.barplot(x="Mode", y="Time", hue="Work Length", data=subset, palette="viridis", ax=axes[i])
            
            axes[i].set_title(f'Time Comparison for {iterations} Iterations with {threads} Threads for {exec_files[index][1]} locks repeated {max_repeat} times')
            axes[i].set_xlabel('Mode')
            axes[i].set_ylabel('Time (seconds)')
            axes[i].legend(title="Work Length", bbox_to_anchor=(1.05, 1), loc='upper left')

            inc_height = min(subset["Time"])/2
            for p in axes[i].patches:
                height = p.get_height()
                if height == 0:
                    continue

                axes[i].text(p.get_x() + p.get_width() / 2, height + inc_height, f'{height:.5f}', ha="center", va="bottom")
            
            limit = inc_y_index[index]*max(subset["Time"]) if max(subset["Time"]) < 1 else max(subset["Time"])+5
            axes[i].set_ylim(0, limit)

        plt.tight_layout()
        output_filename = f"plot_{threads}_threads.png"
        output_file = os.path.join(exec_files[index][1], output_filename)
        plt.savefig(output_file, bbox_inches='tight')  # Save the figure
        print(f"Plot for {threads} threads saved as {output_file}")
        plt.clf()


if __name__ == "__main__":
    os.path.dirname(os.path.realpath(__file__)) 
    subprocess.run(['make'])
    for i in range(1):
        get_perf_data(i)
        collect_data(i)
        gen_graphs(i)
