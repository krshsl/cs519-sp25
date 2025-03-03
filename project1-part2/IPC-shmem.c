/* CS 519, Spring 2025: Project 1 - Part 2
 * IPC using shared memory to perform matrix multiplication.
 * Feel free to extend or change any code or functions below.
 */
#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <sched.h>
#include <semaphore.h>
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
#define MATRIX_SIZE 10000
// floating point precision issues if value exceeds certain thresholds
#define MAX_VALUE (MATRIX_SIZE <= 1024 ? 240 : \
				 (MATRIX_SIZE <= 4096 ? 100 : \
				 (MATRIX_SIZE <= 8192 ? 10 : 4)))
#define MAX_CORES 40
#define BLOCKS_PER_CORE MATRIX_SIZE/MAX_CORES // to ensure semaphores avoid a waiting spree
#define IS_VALIDATE_MATRIX 1

#define MINF(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAXF(X, Y) (((X) > (Y)) ? (X) : (Y))

int maxCores = MINF(MATRIX_SIZE*MATRIX_SIZE, MAX_CORES); // hard limit to not exceed total blocks
char print_mode = 0; // print in csv format for easy parsing

struct grid_block {
	int grid_row;
	int grid_col;
	int block_row;
	int block_col;
	int block_size;
	int total_blocks;
};

void validate_args(int argc, char const **argv);
sem_t* semaphore_create(int blocks, int *shmid);
void destroy_semaphores(sem_t* locks, int num_parts);
double getdetlatimeofday(struct timeval *begin, struct timeval *end);
void print_stats(double time_taken, struct grid_block *gblock);

float random_num(float min, float max);
void init_grid_block(struct grid_block *gblock);
float **init_matrix(int size, unsigned char is_zero);
float *create_shm_matrix(int* shmid);
void print_matrix(float **matrix, int size, const char *msg);
void print_result(float *matrix, int size, const char *msg);
void free_matrix(float **matrix, int size);
void init_workers(float **matrix1, float **matrix2, float *result, sem_t *locks, struct grid_block *gblock);
void child_worker(float **matrix1, float **matrix2, float *result, sem_t *locks, struct grid_block *gblock, int id);
void validate_mult(float **matrix1, float **matrix2, float *result);

void validate_args(int argc, char const **argv) {
	if (MATRIX_SIZE <= 0) {
		printf("Matrix size should be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	if (MAX_VALUE <= 0) {
		printf("Max value should be greater than 0\n");
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

sem_t* semaphore_create(int blocks, int* shmid) {
    size_t size = sizeof(sem_t) * blocks;
    *shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);

    if (*shmid == -1) {
        perror("shmget locks");
        return NULL;
    }
    sem_t* locks = (sem_t*)shmat(*shmid, NULL, 0);

    if (locks == (void *)-1) {
        perror("shmat locks");
        return NULL;
    }
    for (int i = 0; i < blocks; i++) {
        if (sem_init(&locks[i], 1, 1) == -1) {
            perror("sem_init");
            exit(1);
        }
    }
    return locks;
}

void destroy_semaphores(sem_t* locks, int blocks) {
    for (int i = 0; i < blocks; i++) {
        sem_destroy(&locks[i]);
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
	if (print_mode == 1) {
		printf("%d,%d,shmem,%f\n", MATRIX_SIZE, maxCores, time_taken);
		return;
	}

	printf("Input size: %d columns, %d rows\n", MATRIX_SIZE, MATRIX_SIZE);
	printf("Total Cores: %d cores\n", maxCores);
	printf("Total runtime: %f seconds\n", time_taken);
	printf("Blocks used: %dx%d=%d\n", gblock->grid_row, gblock->grid_col, gblock->grid_row*gblock->grid_col);
	printf("Block size: %dx%d\n", gblock->block_row, gblock->block_col);
	printf("Total blocks: %d\n", gblock->total_blocks);
}

float random_num(float min, float max) {
   return min + (int)(rand() / (RAND_MAX / (max - min + 1) + 1));
}

void init_grid_block(struct grid_block *gblock) {
	int min_blocks = BLOCKS_PER_CORE;
	gblock->grid_row = floor(sqrt(min_blocks));
	gblock->grid_col = ceil((float)min_blocks / gblock->grid_row);
	gblock->block_row = ceil((float)MATRIX_SIZE / gblock->grid_row);
	gblock->block_col = ceil((float)MATRIX_SIZE / gblock->grid_col);	
	gblock->block_size = gblock->block_row * gblock->block_row * sizeof(float);
	gblock->total_blocks = gblock->grid_row * gblock->grid_col;
	
	if (gblock->total_blocks < min_blocks) {
		gblock->grid_row++;
		gblock->grid_col++;
		gblock->block_row = ceil((float)MATRIX_SIZE / gblock->grid_row);
		gblock->block_col = ceil((float)MATRIX_SIZE / gblock->grid_col);
		gblock->block_size = gblock->block_row * gblock->block_row * sizeof(float);
		gblock->total_blocks = gblock->grid_row * gblock->grid_col;
	}
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

void print_result(float *matrix, int size, const char *msg) {
	if (size > 5) {
		return;
	}

	printf("%s\n", msg);
	size_t pos;
	for (int i = 0; i < size; i++) {
		pos = i * size;
		for (int j = 0; j < size; j++, pos++) {
			printf("%f\t", matrix[pos]);
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

void child_worker(float **matrix1, float **matrix2, float *result, sem_t *locks, struct grid_block *gblock, int id) {
	// printf("Child %d::%d started\n", id, getpid());
	float *block_buf = malloc(gblock->block_size);
	size_t count, x1, x2, y1, y2, pos, r_pos;
	size_t i, j, k, m, n, l;
	size_t lock_id;
	lock_id = count = 0;
	for (j = 0; j < gblock->grid_col; j++) {
		if (j * gblock->block_col >= MATRIX_SIZE) {
			break;
		}
		for (i = 0; i < gblock->grid_row; i++) {
			if (i * gblock->block_row >= MATRIX_SIZE) {
				break;
			}
			for (k = 0; k < gblock->grid_row; k++, lock_id++) {
				lock_id = lock_id % gblock->total_blocks;
				if (k * gblock->block_row >= MATRIX_SIZE) {
					break;
				}

				if (count++ % maxCores != id) {
					continue;
				}
				memset(block_buf, 0, gblock->block_size);
				 
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

				sem_wait(&locks[lock_id]);
				x1 = i * gblock->block_row;
				for (m = 0; m < gblock->block_row; m++, x1++) {
					if (x1 >= MATRIX_SIZE) {
						break;
					}
					y1 = k * gblock->block_row;
					pos = m * gblock->block_row;
					r_pos = x1 * MATRIX_SIZE + y1;
					for (n = 0; n < gblock->block_row; n++, y1++, r_pos++, pos++) {
						if (y1 >= MATRIX_SIZE) {
							break;
						}
						result[r_pos] += block_buf[pos];
					}
				}
				sem_post(&locks[lock_id]);
			}
		}
	}
	free(block_buf);
	// printf("Child %d::%d finished\n", id, getpid());
	exit(0);
}

void init_workers(float **matrix1, float **matrix2, float *result, sem_t *locks, struct grid_block *gblock) {
	pid_t pid;
	for (int i = 0; i < maxCores; i++) {
		pid = fork();

		if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
			child_worker(matrix1, matrix2, result, locks, gblock, i);
		}
	}
}

void wait_workers() {
	for (int i = 0; i < maxCores; i++) {
		wait(NULL);
	}
}

void validate_mult(float **matrix1, float **matrix2, float *result) {
	if (!IS_VALIDATE_MATRIX || (print_mode == 1) || MATRIX_SIZE > 2049) {
		return;
	}

	float **verify = init_matrix(MATRIX_SIZE, 0);
	size_t pos, count = 0;
	for (int i = 0; i < MATRIX_SIZE; i++) {
		pos = i * MATRIX_SIZE;
		for (int j = 0; j < MATRIX_SIZE; j++, pos++) {
			verify[i][j] = 0;
			for (int k = 0; k < MATRIX_SIZE; k++, count++) {
				verify[i][j] += matrix1[i][k] * matrix2[k][j];
			}
			
			if (fabs(verify[i][j] - result[pos]) > 0.0001) {
				printf("Validation failed at %d %d\n", i, j);
				printf("Expected: %f, Actual: %f\n", verify[i][j], result[pos]);
				exit(EXIT_FAILURE);
			}
			
			if (count > 16*MATRIX_SIZE*MATRIX_SIZE) // verify a subset of the matrix will be enough
				break;
		}

		if (count > 16*MATRIX_SIZE*MATRIX_SIZE)
			break;
	}
	print_matrix(verify, MATRIX_SIZE, "Actual Matrix");
	free_matrix(verify, MATRIX_SIZE);
}

float *create_shm_matrix(int* shmid) {
    size_t size = sizeof(float) * MATRIX_SIZE * MATRIX_SIZE;
    *shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
    
	if (*shmid == -1) {
        perror("shmget data");
        return NULL;
    }
    float *result = (float*)shmat(*shmid, NULL, 0);

    if (result == (void *)-1) {
        perror("shmat data");
        return NULL;
    }
    memset(result, 0, size);
    return result;
}

int main(int argc, char const *argv[])
{
	validate_args(argc, argv);
	int res_id, locks_id;
	double time_taken = 0;
	struct timeval begin, end;
	float **matrix1 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix1, MATRIX_SIZE, "Matrix 1");
	float **matrix2 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix2, MATRIX_SIZE, "Matrix 2");
	float *result = create_shm_matrix(&res_id);
	struct grid_block gblock;
	init_grid_block(&gblock);
	sem_t* locks = semaphore_create(gblock.total_blocks, &locks_id);


	// time taken to perform matrix multiplication related tasks
	gettimeofday(&begin, NULL);
	init_workers(matrix1, matrix2, result, locks, &gblock);
	wait_workers();
	print_result(result, MATRIX_SIZE, "Result");
	gettimeofday(&end, NULL);
	time_taken = getdetlatimeofday(&begin, &end);
	print_stats(time_taken, &gblock);

	// validate the result and free the memory
	gettimeofday(&begin, NULL);
	validate_mult(matrix1, matrix2, result);
	gettimeofday(&end, NULL);
	free_matrix(matrix1, MATRIX_SIZE);
	free_matrix(matrix2, MATRIX_SIZE);
	destroy_semaphores(locks, gblock.total_blocks);
    shmdt(result);
    shmdt(locks);
    shmctl(res_id, IPC_RMID, NULL);
    shmctl(locks_id, IPC_RMID, NULL);
	return 0;
}


