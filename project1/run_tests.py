from re import sub
from subprocess import run
from time import sleep
import pandas as pd
import matplotlib.pyplot as plt

data_types = {
    'char': 1,
    'unsigned char': 1,
    'int': 4,
    'unsigned int': 4,
    'long': 8,
    'unsigned long': 8
}

modes = ['single', 'thread']
sizes = [2**i for i in range(8, 17)] # 256 to 65536

def update_content(content, data_type, thread_count, size):
    data_size = size // data_types[data_type]
    content = sub(r'#define\s+DATA_TYPE\s+\w+(?:\s+\w+)?', f'#define DATA_TYPE {data_type}', content)
    content = sub(r'#define\s+DATA_SIZE\s+\d+', f'#define DATA_SIZE {data_size}', content)
    content = sub(r'#define\s+THREADS\s+\d+', f'#define THREADS {thread_count}', content)
    return content

def run_test(mode, oFile):
    run(['make', 'clean'], capture_output=True, text=True)
    run(['make'], capture_output=True, text=True)
    oFile.write(run(['./test.o', mode, 'csv'], capture_output=True, text=True).stdout)

def run_tests(oFile):
    with open('test.c', 'r') as file:
        content = file.read()

    for data_type in data_types:
        for mode in modes:
            if mode == 'single':
                threads = [1]
            else:
                threads = [10, 20, 40, 80]

            for thread in threads:
                for size in sizes:
                    curr_test = f"Running test for {data_type} and size {size}"
                    print(curr_test)
                    oFile.write(f"{data_type},{size},")
                    updated_content = update_content(content, data_type, thread, size)
                    with open('test.c', 'w') as file:
                        file.write(updated_content)
                    
                    run_test(mode, oFile)
                    sleep(1) # Sleep for 1 second to avoid overloading the system
            

def reset():
    with open('test.c', 'r') as file:
        content = file.read()

    updated_content = update_content(content, 'char', 40, 1024)
    with open('test.c', 'w') as file:
        file.write(updated_content)

def plot_graph(color_index, curr_df):
    colors = plt.cm.Paired.colors

    for data_type in curr_df['Data Type'].unique():
        subset = curr_df[curr_df['Data Type'] == data_type]
        plt.plot(subset['Size (Bytes)'], subset['Avg Time Taken(microseconds)'], label=f'{data_type}', color=colors[color_index], linewidth=0.5)
        color_index = (color_index + 1) % len(colors)
    
    avg_times = curr_df.groupby('Size (Bytes)')['Avg Time Taken(microseconds)'].mean()
    plt.plot(avg_times.index, avg_times.values, label='Average', linestyle='--', color='black', linewidth=1)
    
    for size in avg_times.index:
        plt.axvline(x=size, color='gray', linestyle=':', linewidth=0.5)

def generate_graph():
    df = pd.read_csv('output.csv')
    for mode in modes:
        mode_df = df[df['Mode'] == mode]
        
        thread_counts = mode_df['Count'].unique() if mode == 'thread' else [None]
        for count in thread_counts:
            plt.figure()
            color_index = 0
            count_df = mode_df[mode_df['Count'] == count] if count is not None else mode_df
            optional = f' (threads={count})' if count is not None else ''
            plot_graph(color_index, count_df)
            plt.xlabel('Size (Bytes)')
            plt.ylabel('Avg Time Taken (microseconds)')
            plt.title(f'Performance Comparison by Data Type and Size{optional}')
            plt.legend()
            plt.grid(True)
            filename = f'performance_comparison_threads_{count}.png' if count is not None else f'performance_comparison_{mode}.png'
            plt.savefig(filename)
            plt.show()

if __name__ == '__main__':
    with open('output.csv', 'w') as oFile:
        oFile.write('Data Type,Size (Bytes),Iterations,Success Rate,Failure Rate,Avg Time Taken(microseconds),Mode,Count\n')
        run_tests(oFile)

    reset()
    generate_graph()
