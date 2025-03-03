/* CS 519, Spring 2025: Project 1 - Part 2
 * IPC using pipes to perform matrix multiplication.
 * Feel free to extend or change any code or functions below.
 */
#define _GNU_SOURCE
#include <math.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

//Add all your global variables and definitions here.
#define MATRIX_SIZE 2048
#define MAX_BLOCK_SIZE 64 // hard limit for block size to not exceed 32k bytes
#define MAX_VALUE (MATRIX_SIZE <= 1024 ? 1000 : \
                 (MATRIX_SIZE <= 4096 ? 100 : \
                 (MATRIX_SIZE <= 8192 ? 10 : 4)))
#define MAX_PROCESS 40

#define MINF(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAXF(X, Y) (((X) > (Y)) ? (X) : (Y))

int maxCores = MINF(MATRIX_SIZE*MATRIX_SIZE, MAX_PROCESS); // hard limit on max process to not exceed total matrix elements
char print_mode = 0; // print in csv format for easy parsing

struct grid_block {
	int grid_row;
	int grid_col;
	int block_row;
	int block_col;
	int block_size;
};

void validate_args(int argc, char const **argv);
double getdetlatimeofday(struct timeval *begin, struct timeval *end);
void print_stats(double time_taken, struct grid_block *gblock);

float random_num(float min, float max);
void init_grid_block(struct grid_block *gblock);
float **init_matrix(int size, unsigned char is_zero);
void print_matrix(float **matrix, int size, const char *msg);
void free_matrix(float **matrix, int size);
void init_pipes(int fd[maxCores][2]);
void matrix_mult(float **matrix1, float **matrix2, int fd[maxCores][2], struct grid_block *gblock, int id);
void init_workers(float **matrix1, float **matrix2, int fd[maxCores][2], struct grid_block *gblock);
void child_worker(float **matrix1, float **matrix2, int fd[maxCores][2], struct grid_block *gblock, int id);
void process_blocks(struct grid_block *gblock, int fd[maxCores][2], float **result);
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

	if (MAX_PROCESS <= 0) {
		printf("Max process should be greater than 0\n");
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
void print_stats(double time_taken, struct grid_block *gblock) {
	printf("Input size: %d columns, %d rows\n", MATRIX_SIZE, MATRIX_SIZE);
	printf("Total Cores: %d cores\n", maxCores);
	printf("Total runtime: %f seconds\n", time_taken);
	printf("Blocks used: %dx%d=%d\n", gblock->grid_row, gblock->grid_col, gblock->grid_row*gblock->grid_col);
	printf("Block size: %dx%d\n", gblock->block_row, gblock->block_col);
}

float random_num(float min, float max) {
   return min + (int)(rand() / (RAND_MAX / (max - min + 1) + 1));
}

void init_grid_block(struct grid_block *gblock) {
	do {
		gblock->grid_row = floor(sqrt(maxCores));
		while (maxCores % gblock->grid_row != 0) {
			gblock->grid_row--;
		}
		gblock->grid_col = maxCores / gblock->grid_row;
		gblock->block_row = ceil((float)MATRIX_SIZE / gblock->grid_row);
		gblock->block_col = ceil((float)MATRIX_SIZE / gblock->grid_col);
		if (gblock->block_row > MAX_BLOCK_SIZE || gblock->block_col > MAX_BLOCK_SIZE) {
			int min_grid_row = ceil((float)MATRIX_SIZE / MAX_BLOCK_SIZE);
			int min_grid_col = ceil((float)MATRIX_SIZE / MAX_BLOCK_SIZE);
			gblock->grid_row = MAXF(gblock->grid_row, min_grid_row);
			gblock->grid_col = MAXF(gblock->grid_col, min_grid_col);
			gblock->block_row = ceil((float)MATRIX_SIZE / gblock->grid_row);
			gblock->block_col = ceil((float)MATRIX_SIZE / gblock->grid_col);
		}
	} while (gblock->block_row > MAX_BLOCK_SIZE || gblock->block_col > MAX_BLOCK_SIZE);
	gblock->block_size = gblock->block_row * gblock->block_row * sizeof(float);
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
	if (size > 5) {
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

void matrix_mult(float **matrix1, float **matrix2, int fd[maxCores][2], struct grid_block *gblock, int id) {
	float *block_buf = malloc(gblock->block_size);
	int count, x1, x2, y1, y2, pos;
	int i, j, k, m, n, l;
	count = 0;
	for (j = 0; j < gblock->grid_col; j++) {
		if (j * gblock->block_col >= MATRIX_SIZE) {
			break;
		}
		for (i = 0; i < gblock->grid_row; i++) {
			if (i * gblock->block_row >= MATRIX_SIZE) {
				break;
			}
			for (k = 0; k < gblock->grid_row; k++) {
				if (k * gblock->block_row >= MATRIX_SIZE) {
					break;
				}

				if (count++ % maxCores != id) {
					continue;
				}
				memset(block_buf, 0, gblock->block_size);
				 
				// process for ij * jk -> ik
				x1 = i * gblock->block_row;
				for (m = 0; m < gblock->block_row; m++, x1++) {
					if (x1 >= MATRIX_SIZE) {
						break;
					}
					y2 = k * gblock->block_row;
					pos = m * gblock->block_row;
					for (n = 0; n < gblock->block_row; n++, y2++, pos++) {
						if (y2 >= MATRIX_SIZE) {
							break;
						}
						x2 = y1 = j * gblock->block_col;
						for (l = 0; l < gblock->block_col; l++, x2++, y1++) {
							if (y1 >= MATRIX_SIZE || x2 >= MATRIX_SIZE) {
								break;
							}
							// printf("child %d::%d,%d,%d\n", id, m, n, l);
							block_buf[pos] += matrix1[x1][y1] * matrix2[x2][y2];
						}
					}
				}
				ssize_t bytes_written = write(fd[id][1], block_buf, gblock->block_size);
				while (bytes_written < gblock->block_size) {
					bytes_written += write(fd[id][1], block_buf + bytes_written, gblock->block_size - bytes_written);
					if (bytes_written < 0) {
						perror("write failed");
						exit(EXIT_FAILURE);
					}

					if (bytes_written < sizeof(float)) {
						sched_yield();
					}
				}
    			// printf("Child %d wrote %zd bytes\n", id, bytes_written);
			}
		}
	}
	free(block_buf);
}

void child_worker(float **matrix1, float **matrix2, int fd[maxCores][2], struct grid_block *gblock, int id) {
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
	matrix_mult(matrix1, matrix2, fd, gblock, id);
	if(close(fd[id][1]) == -1) {
		perror("close write failed - child");
		exit(EXIT_FAILURE);
	}
	// printf("Child %d::%d finished\n", id, getpid());
	exit(0);
}

void init_workers(float **matrix1, float **matrix2, int fd[maxCores][2], struct grid_block *gblock) {
	pid_t pid;
	for (int i = 0; i < maxCores; i++) {
		pid = fork();

		if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
			child_worker(matrix1, matrix2, fd, gblock, i);
		}
	}
}

void wait_workers() {
	for (int i = 0; i < maxCores; i++) {
		wait(NULL);
	}
}

void process_blocks(struct grid_block *gblock, int fd[maxCores][2], float **result) {
	int count, pos, rx, ry;
	int i, j, k, m, n;
	float *block_buf = malloc(gblock->block_size);
	count = 0;
	for (j = 0; j < gblock->grid_col; j++) {
		for (i = 0; i < gblock->grid_row; i++) {
			for (k = 0; k < gblock->grid_row; k++) {
				ssize_t bytes_read = 0;
				while(bytes_read < gblock->block_size) {
					bytes_read += read(fd[count][0], block_buf + bytes_read, gblock->block_size - bytes_read);
					if (bytes_read < 0) {
						perror("read failed");
						exit(EXIT_FAILURE);
					}

					if (bytes_read < gblock->block_size) {
						sched_yield();
					}
				}

				// maybe do the summation by sending data to threads using splice and getting it back??
				rx = i * gblock->block_row;
				for (m = 0; m < gblock->block_row; m++, rx++) {
					if (rx >= MATRIX_SIZE) {
						break;
					}
					ry = k * gblock->block_row;
					pos = m * gblock->block_row;
					for (n = 0; n < gblock->block_row; n++, ry++, pos++) {
						if (ry >= MATRIX_SIZE) {
							break;
						}
						// printf("Parent::%d,%d,%d,%d\n", rx, ry, pos, (int)block_buf[pos]);
						result[rx][ry] += block_buf[pos];
					}
				}
				count = (count + 1) % maxCores;
			}
		}
	}
	free(block_buf);
}

void validate_mult(float **matrix1, float **matrix2, float **result) {
	if (MATRIX_SIZE > 2049) {
		return;
	}

	float **verify = init_matrix(MATRIX_SIZE, 0);
	for (int i = 0; i < MATRIX_SIZE; i++) {
		for (int j = 0; j < MATRIX_SIZE; j++) {
			verify[i][j] = 0;
			for (int k = 0; k < MATRIX_SIZE; k++) {
				verify[i][j] += matrix1[i][k] * matrix2[k][j];
			}
			
			if (fabs(verify[i][j] - result[i][j]) > 0.0001) {
				printf("Validation failed at %d %d\n", i, j);
				printf("Expected: %f, Actual: %f\n", verify[i][j], result[i][j]);
				exit(EXIT_FAILURE);
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
	struct grid_block gblock;
	int fd[maxCores][2];
	
	// time taken to perform matrix multiplication related tasks
	gettimeofday(&begin, NULL);
	init_pipes(fd);
	init_grid_block(&gblock);
	init_workers(matrix1, matrix2, fd, &gblock);
	for (int i = 0; i < maxCores; i++) {
		if (close(fd[i][1]) == -1) {
			perror("close write failed - parent");
			exit(EXIT_FAILURE);
		}
	}
	float **result = init_matrix(MATRIX_SIZE, 0); // avoiding init at start to save some memory for forked processes
	process_blocks(&gblock, fd, result);
	print_matrix(result, MATRIX_SIZE, "Result");
	for (int i = 0; i < maxCores; i++) {
		if (close(fd[i][0]) == -1) {
			perror("close read failed - parent");
			exit(EXIT_FAILURE);
		}
	}
	gettimeofday(&end, NULL);
	time_taken = getdetlatimeofday(&begin, &end);
	print_stats(time_taken, &gblock);

	// validate the result and free the memory
	gettimeofday(&begin, NULL);
	validate_mult(matrix1, matrix2, result);
	gettimeofday(&end, NULL);
	free_matrix(matrix1, MATRIX_SIZE);
	free_matrix(matrix2, MATRIX_SIZE);
	free_matrix(result, MATRIX_SIZE);
	wait_workers(); // not really required as all children are exited
	
	if (MATRIX_SIZE <= 1024 && print_mode == 0) {
		gettimeofday(&end, NULL);
		time_taken = getdetlatimeofday(&begin, &end);
		print_stats(time_taken, &gblock);
	}
	return 0;
}

/*
	Reference...

	00 01 02
	10 11 12
	20 21 22

	2x1
	00 01 02 03
	10 11 12 13
	20 21 22 23
	30 31 32 33

	1x2
	00 01 02 03
	10 11 12 13
	20 21 22 23
	30 31 32 33

	00 01
	10 11

	Process Matrix reference :: (avoid locking often...)
	i = 0, j = 0, k = 0 -> 00
	00 x 00 -> 00, 01, 10, 11 & 00, 01, 10, 11
	01 x 10 -> 02, 03, 12, 13 & 20, 21, 30, 31

	i = 0, j = 1, k = 0 -> 01
	00 x 01 -> 00, 01, 10, 11 & 02, 03, 12, 13
	01 x 11 -> 02, 03, 12, 13 & 21, 22, 31, 32

	i = 1, j = 0, k = 0 -> 10
	10 x 00 -> 20, 21, 30, 31 & 00, 01, 10, 11
	11 x 10 -> 22, 23, 32, 33 & 20, 21, 30, 31

	i = 1, j = 1, k = 0 -> 11
	10 x 01 -> 20, 21, 30, 31 & 02, 03, 12, 13
	11 x 11 -> 22, 23, 32, 33 & 21, 22, 31, 32
		
	i = 0, j = 0, k = 0
	00 x 00 -> ji x ik -> 00 -> jk
	00 x 01 -> ji x ik -> 01 -> jk

	i = 0, j = 1, k = 0
	10 x 00 -> ji x ik -> 10 -> jk
	10 x 01 -> ji x ik -> 11 -> jk

	i = 1, j = 0, k = 0
	01 x 10 -> ji x ik -> 00 -> jk 
	01 x 11 -> ji x ik -> 01 -> jk

	i = 1, j = 1, k = 0
	11 x 10 -> ji x ik -> 10 -> jk
	11 x 11 -> ji x ik -> 11 -> jk
	
	00,01,02
	10,11,12
	20,21,22

	3x3 matrix into 3x2 * 2x3 matrix with 1x2 & 2x1 blocks
	m1 -> m2 -> r_pos (rowxcol,1x2,2x1)
	
	update ij * jk -> ik

	00,01 -> 00,10 -> 00
	00,01 -> 01,11 -> 01
	00,01 -> 02,12 -> 02
	00,01 -> 03,13 -> 03 (but not needed)

	10,11 -> 00,10 -> 10
	10,11 -> 01,11 -> 11
	10,11 -> 02,12 -> 12
	10,11 -> 03,13 -> 13 (but not needed)
	
	20,21 -> 00,10 -> 20
	20,21 -> 01,11 -> 21
	20,21 -> 02,12 -> 22
	20,21 -> 03,13 -> 23 (but not needed)


	02,03 -> 20,30 -> 00
	02,03 -> 21,31 -> 01
	02,03 -> 22,32 -> 02
	02,03 -> 23,33 -> 03 (but not needed)

	12,13 -> 20,30 -> 10
	12,13 -> 21,31 -> 11
	12,13 -> 22,32 -> 12
	12,13 -> 23,33 -> 13 (but not needed)

	22,23 -> 20,30 -> 20
	22,23 -> 21,31 -> 21
	22,23 -> 22,32 -> 22
	22,23 -> 23,33 -> 23 (but not needed)
*/