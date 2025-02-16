from re import sub
from subprocess import run

data_types = {
    'char': 1,
    'unsigned char': 1,
    'int': 4,
    'unsigned int': 4,
    'long': 8,
    'unsigned long': 8
}

sizes = [2**i for i in range(8, 17)] # 256 to 65536

def update_content(content, data_type, size):
    data_size = size // data_types[data_type]
    content = sub(r'#define\s+DATA_TYPE\s+\w+(?:\s+\w+)?', f'#define DATA_TYPE {data_type}', content)
    content = sub(r'#define\s+DATA_SIZE\s+\d+', f'#define DATA_SIZE {data_size}', content)
    return content

def run_test(oFile):
    run(['make', 'clean'], capture_output=True, text=True)
    run(['make'], capture_output=True, text=True)
    oFile.write(run(['./test.o', 'csv'], capture_output=True, text=True).stdout)

def run_tests(oFile):
    with open('test.c', 'r') as file:
        content = file.read()

    for data_type in data_types:
        for size in sizes:
            curr_test = f"Running test for {data_type} and size {size}"
            print(curr_test)
            oFile.write(f"{data_type},{size}")
            updated_content = update_content(content, data_type, size)
            with open('test.c', 'w') as file:
                file.write(updated_content)
            
            run_test(oFile)
            

def reset():
    with open('test.c', 'r') as file:
        content = file.read()

    updated_content = update_content(content, 'char', 1024)
    with open('test.c', 'w') as file:
        file.write(updated_content)

if __name__ == '__main__':
    with open('output.txt', 'w') as oFile:
        oFile.write('Data Type,Size,Iterations,Success Rate,Failure Rate,Avg Time Taken(microseconds)\n')
        run_tests(oFile)

    reset()
