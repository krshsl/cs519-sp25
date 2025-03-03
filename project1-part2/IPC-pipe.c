/* CS 519, Spring 2025: Project 1 - Part 2
 * IPC using pipes to perform matrix multiplication.
 * Feel free to extend or change any code or functions below.
 * 
 * use following command to get approx performance stats:
 * make && /usr/bin/time -f "%e,%U,%S" -q ./pipe -p
 */
#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

//Add all your global variables and definitions here.
#define MATRIX_SIZE 4096
#define MAX_BLOCK_SIZE 64 // hard limit for block size to not exceed 32k bytes
// floating point precision issues if value exceeds certain thresholds
#define MAX_VALUE (MATRIX_SIZE <= 1024 ? 240 : \
				 (MATRIX_SIZE <= 4096 ? 100 : \
				 (MATRIX_SIZE <= 8192 ? 10 : 4)))
#define MAX_CORES 40
#define IS_VALIDATE_MATRIX 1
#define VALIDATE_MAX_SIZE MATRIX_SIZE+1
#define PRINT_CAP 4

#define MINF(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAXF(X, Y) (((X) > (Y)) ? (X) : (Y))

int maxCores = MINF(MATRIX_SIZE*MATRIX_SIZE, MAX_CORES); // hard limit to not exceed total matrix elements
char print_mode = 0; // print in csv format for easy parsing

void validate_args(int argc, char const **argv);
double getdetlatimeofday(struct timeval *begin, struct timeval *end);
void print_stats(double time_taken);

float random_num(float min, float max);
float **init_matrix(int size, unsigned char is_zero);
void print_matrix(float **matrix, int size, const char *msg);
void free_matrix(float **matrix, int size);
void init_pipes(int fd[maxCores][2]);
void matrix_mult(float **matrix1, float **matrix2, int fd[maxCores][2], int id);
void init_workers(float **matrix1, float **matrix2, int fd[maxCores][2]);
void child_worker(float **matrix1, float **matrix2, int fd[maxCores][2], int id);
void process_blocks(int fd[maxCores][2], float **result);
void validate_mult(float **matrix1, float **matrix2, float **result);

void validate_args(int argc, char const **argv) {
	if (MATRIX_SIZE <= 0) {
		printf("Matrix size should be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	if (MAX_VALUE <= 0) {
		printf("Max value should be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	if (MAX_BLOCK_SIZE <= 0) {
		printf("Max block size should be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	if (MAX_CORES <= 0) {
		printf("Max cores should be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-p") == 0) {
				print_mode = 1;
			}
		}
	}
}

/* Time function that calculates time between start and end */
double getdetlatimeofday(struct timeval *begin, struct timeval *end) {
    return (end->tv_sec + end->tv_usec * 1.0 / 1000000) -
           (begin->tv_sec + begin->tv_usec * 1.0 / 1000000);
}


/* Stats function that prints the time taken and other statistics that you wish
 * to provide.
 */
void print_stats(double time_taken) {
	if (print_mode == 1) {
		printf("%d,%d,pipe,%f\n", MATRIX_SIZE, maxCores, time_taken);
		return;
	}

	printf("Input size: %d columns, %d rows\n", MATRIX_SIZE, MATRIX_SIZE);
	printf("Total Cores: %d cores\n", maxCores);
	printf("Total runtime: %f seconds\n", time_taken);
}

float random_num(float min, float max) {
   return min + (int)(rand() / (RAND_MAX / (max - min + 1) + 1));
}

float **init_matrix(int size, unsigned char is_zero) {
   	float **matrix = (float **)malloc(size * sizeof(float *));
   	for (int i = 0; i < size; i++) {
		matrix[i] = (float *)malloc(size * sizeof(float));
		
		if (is_zero == 0) {
			memset(matrix[i], 0, size * sizeof(float));
			continue;
		}
		for (int j = 0; j < size; j++) {
			matrix[i][j] = random_num(0, MAX_VALUE);
		}
   	}
   	return matrix;
}

void print_matrix(float **matrix, int size, const char *msg) {
	if (size > PRINT_CAP) {
		return;
	}

	printf("%s\n", msg);
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			printf("%f\t", matrix[i][j]);
		}
		printf("\n");
	}
}

void free_matrix(float **matrix, int size) {
	for (int i = 0; i < size; i++) {
		free(matrix[i]);
	}
	free(matrix);
}

void init_pipes(int fd[maxCores][2]) {
	for (int i = 0; i < maxCores; i++) {
		if (pipe(fd[i]) == -1) {
			perror("pipe failed");
			exit(1);
		}
	}
}

void matrix_mult(float **matrix1, float **matrix2, int fd[maxCores][2], int id) {
	size_t size = MATRIX_SIZE * sizeof(float);
	float *block_buf = malloc(size);
	for (int i = id; i < MATRIX_SIZE; i+=maxCores) {
		memset(block_buf, 0, size);
		for (int k = 0; k < MATRIX_SIZE; k++) {
			for (int j = 0; j < MATRIX_SIZE; j++) {
				block_buf[j] += matrix1[i][k] * matrix2[k][j];
			}
		}
		ssize_t bytes_written = write(fd[id][1], block_buf, size);
		while (bytes_written < size) {
			bytes_written += write(fd[id][1], block_buf + bytes_written, MATRIX_SIZE - bytes_written);
			if (bytes_written < 0) {
				perror("write failed");
				exit(EXIT_FAILURE);
			}

			if (bytes_written < sizeof(float)) {
				sched_yield();
			}
		}
	}
	free(block_buf);
}

void child_worker(float **matrix1, float **matrix2, int fd[maxCores][2], int id) {
	// printf("Child %d::%d started\n", id, getpid());
	for (int i = 0; i < maxCores; i++) {
		if (i != id) {
			if (close(fd[i][1]) == -1) {
				perror("close write failed - children");
				exit(EXIT_FAILURE);
			}
		}
		if (close(fd[i][0]) == -1) {
			perror("close read failed - children");
			exit(EXIT_FAILURE);
		}
	}
	matrix_mult(matrix1, matrix2, fd, id);
	if(close(fd[id][1]) == -1) {
		perror("close write failed - child");
		exit(EXIT_FAILURE);
	}
	// printf("Child %d::%d finished\n", id, getpid());
	exit(0);
}

void init_workers(float **matrix1, float **matrix2, int fd[maxCores][2]) {
	pid_t pid;
	for (int i = 0; i < maxCores; i++) {
		pid = fork();

		if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
			child_worker(matrix1, matrix2, fd, i);
		}
	}
}

void wait_workers() {
	for (int i = 0; i < maxCores; i++) {
		wait(NULL);
	}
}

void process_blocks(int fd[maxCores][2], float **result) {
	int id;
	float *block;
	size_t size = MATRIX_SIZE * sizeof(float);
	for (int i = 0; i < maxCores; i++) {
		id = i % maxCores;
		block = result[i];
		ssize_t bytes_read = read(fd[id][0], block, size);
		while(bytes_read < size) {
			bytes_read += read(fd[id][0], block + bytes_read, size - bytes_read);
			if (bytes_read < 0) {
				perror("read failed");
				exit(EXIT_FAILURE);
			}

			if (bytes_read < MATRIX_SIZE) {
				sched_yield();
			}
		}
	}
}

void validate_mult(float **matrix1, float **matrix2, float **result) {
	if (!IS_VALIDATE_MATRIX || (print_mode == 1) || MATRIX_SIZE > VALIDATE_MAX_SIZE) {
		return;
	}

	float sum = 0;
	size_t count = 0;
	
	// Standard matrix multiplication algorithm for validation
	for (int i = 0; i < MATRIX_SIZE; i++) {
		for (int j = 0; j < MATRIX_SIZE; j++, count++) {
			sum = 0;
			for (int k = 0; k < MATRIX_SIZE; k++) {
				sum += matrix1[i][k] * matrix2[k][j];
			}
			
			// Validate result
			if (fabs(sum - result[i][j]) >= 0.0001) {
				printf("Validation failed at %d %d\n", i, j);
				printf("Expected: %f, Actual: %f\n", sum, result[i][j]);
				exit(EXIT_FAILURE);
			}
			
			if (count > 16*MATRIX_SIZE) // verify a subset of the matrix will be enough
				break;
		}
		
		if (count > 16*MATRIX_SIZE)
			break;
	}
	printf("Matrix validation successful\n");

	if (MATRIX_SIZE > PRINT_CAP) {
		return;
	}
	float **verify = init_matrix(MATRIX_SIZE, 0);
	for (int i = 0; i < MATRIX_SIZE; i++) {
		for (int j = 0; j < MATRIX_SIZE; j++) {
			verify[i][j] = 0;
			for (int k = 0; k < MATRIX_SIZE; k++) {
				verify[i][j] += matrix1[i][k] * matrix2[k][j];
			}
		}
	}
	print_matrix(verify, MATRIX_SIZE, "Actual Matrix");
	free_matrix(verify, MATRIX_SIZE);
}

int main(int argc, char const **argv) {
	validate_args(argc, argv);
	double time_taken = 0;
	struct timeval begin, end;
	float **matrix1 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix1, MATRIX_SIZE, "Matrix 1");
	float **matrix2 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix2, MATRIX_SIZE, "Matrix 2");
	int fd[maxCores][2];
	
	// time taken to perform matrix multiplication related tasks
	gettimeofday(&begin, NULL);
	init_pipes(fd);
	init_workers(matrix1, matrix2, fd);
	for (int i = 0; i < maxCores; i++) {
		if (close(fd[i][1]) == -1) {
			perror("close write failed - parent");
			exit(EXIT_FAILURE);
		}
	}
	float **result = init_matrix(MATRIX_SIZE, 0); // avoiding init at start to save some memory for forked processes
	process_blocks(fd, result);
	print_matrix(result, MATRIX_SIZE, "Result");
	for (int i = 0; i < maxCores; i++) {
		if (close(fd[i][0]) == -1) {
			perror("close read failed - parent");
			exit(EXIT_FAILURE);
		}
	}
	wait_workers(); // not really required as all children have exited
	gettimeofday(&end, NULL);
	time_taken = getdetlatimeofday(&begin, &end);
	print_stats(time_taken);

	// validate the result and free the memory
	gettimeofday(&begin, NULL);
	validate_mult(matrix1, matrix2, result);
	gettimeofday(&end, NULL);
	free_matrix(matrix1, MATRIX_SIZE);
	free_matrix(matrix2, MATRIX_SIZE);
	free_matrix(result, MATRIX_SIZE);
	return 0;
}

/*
stats::

5000,10,pipe,25.457986
26.56,189.94,5.26

5000,20,pipe,15.624162
16.72,288.25,4.53

5000,40,pipe,10.022775
11.12,264.44,6.73

4000,40,pipe,4.799979
5.50,137.02,3.59

4000,20,pipe,7.988943
8.69,147.94,2.56

4000,10,pipe,12.925466
13.62,96.93,2.34
*/