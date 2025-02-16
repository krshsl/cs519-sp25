from re import sub
from subprocess import run

data_types = {
        'char': 1,
        'int': 4,
        'unsigned int': 4,
        'unsigned char': 1,
        'long': 8,
        'unsigned long': 8
    }

sizes = [256, 512, 1024, 2048, 1024 * 256, 1024 * 512, 1024 * 1024]

def update_content(content, data_type, size):
    data_size = size // data_types[data_type]
    content = sub(r'#define\s+DATA_TYPE\s+\w+', f'#define DATA_TYPE {data_type}', content)
    content = sub(r'#define\s+DATA_SIZE\s+\d+', f'#define DATA_SIZE {data_size}', content)
    return content

def run_test():
    run(['make', 'clean'])
    run(['make'])
    run(['./test.o'])

def run_tests():
    with open('test.c', 'r') as file:
        content = file.read()

    for data_type in data_types:
        for size in sizes:
            print(f"Running test for {data_type} and size {size}")
            updated_content = update_content(content, data_type, size)
            with open('test.c', 'w') as file:
                file.write(updated_content)
            
            run_test()
            

if __name__ == '__main__':
    run_tests()
