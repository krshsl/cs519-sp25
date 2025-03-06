/* CS 519, Spring 2025: Project 1 - Part 2
 * IPC using shared memory to perform matrix multiplication.
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
#include <time.h>
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
#define MAX_VALUE (MATRIX_SIZE < 1024 ? 2048 : \
				 (MATRIX_SIZE < 4096 ? 1024 : \
				 (MATRIX_SIZE <= 8192 ? 512 : 4)))
#define MAX_CORES 16
#define IS_VALIDATE_MATRIX 1
#define VALIDATE_MAX_SIZE 10000
#define PRINT_CAP 4

#define MINF(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAXF(X, Y) (((X) > (Y)) ? (X) : (Y))

int maxCores = MINF(MATRIX_SIZE, MAX_CORES); // hard limit to not size of matrix
int print_mode = 0; // print in csv format for easy parsing

void validate_args(int argc, char const **argv);
double getdetlatimeofday(struct timeval *begin, struct timeval *end);
void print_stats(double time_taken);

float random_num(float min, float max);
float **init_matrix(int size, unsigned char is_zero);
float *create_shm_matrix(int* shmid);
void print_matrix(float **matrix, int size, const char *msg);
void print_result(float *matrix, int size, const char *msg);
void free_matrix(float **matrix, int size);
void init_workers(float **matrix1, float **matrix2, float *result);
void child_worker(float **matrix1, float **matrix2, float *result, int id);
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

/* Time function that calculates time between start and end */
double getdetlatimeofday(struct timeval *begin, struct timeval *end) {
    return (end->tv_sec + end->tv_usec * 1.0 / 1000000) -
           (begin->tv_sec + begin->tv_usec * 1.0 / 1000000);
}


/* Stats function that prints the time taken and other statistics that you wish
 * to provide.
 */
void print_stats(double time_taken) {
	if (print_mode) {
		printf("%d,%d,shmem,%f\n", MATRIX_SIZE, maxCores, time_taken);
		return;
	}

	printf("Input size: %d columns, %d rows\n", MATRIX_SIZE, MATRIX_SIZE);
	printf("Total Cores: %d cores\n", maxCores);
	printf("Total runtime: %f seconds\n", time_taken);
}

float random_num(float min, float max) {
	float result = min + ((float)rand() / RAND_MAX) * (max - min);
	return result;
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

void print_result(float *matrix, int size, const char *msg) {
	if (size > PRINT_CAP) {
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

void child_worker(float **matrix1, float **matrix2, float *result, int id) {
	// printf("Child %d::%d started\n", id, getpid());
	int end = MATRIX_SIZE/8;
	for (int i = id; i < MATRIX_SIZE; i+=maxCores) {
		int k = 0;
		for (; k < end; k+= 8) {
			size_t pos = i*MATRIX_SIZE;
			float m1 = matrix1[i][k] + matrix1[i][k+1] + matrix1[i][k+2] + matrix1[i][k+3] + matrix1[i][k+4] + matrix1[i][k+5] + matrix1[i][k+6] + matrix1[i][k+7];
			int j = 0;
			for (; j < end; j+=8, pos+=8) {
				float m21 = matrix2[k][j];
				float m22 = matrix2[k][j+1];
				float m23 = matrix2[k][j+2];
				float m24 = matrix2[k][j+3];
				float m25 = matrix2[k][j+4];
				float m26 = matrix2[k][j+5];
				float m27 = matrix2[k][j+6];
				float m28 = matrix2[k][j+7];
				result[pos] += m1 * m21;
				result[pos+1] += m1 * m22;
				result[pos+2] += m1 * m23;
				result[pos+3] += m1 * m24;
				result[pos+4] += m1 * m25;
				result[pos+5] += m1 * m26;
				result[pos+6] += m1 * m27;
				result[pos+7] += m1 * m28;
			}

			for (; j < MATRIX_SIZE; j++, pos++) {
				float m2 = matrix2[k][j];
				result[pos] += m1 * m2;
			}
		}

		for (; k < MATRIX_SIZE; k++) {
			size_t pos = i*MATRIX_SIZE;
			float m1 = matrix1[i][k];
			int j = 0;
			for (; j < end; j+=8, pos+=8) {
				float m21 = matrix2[k][j];
				float m22 = matrix2[k][j+1];
				float m23 = matrix2[k][j+2];
				float m24 = matrix2[k][j+3];
				float m25 = matrix2[k][j+4];
				float m26 = matrix2[k][j+5];
				float m27 = matrix2[k][j+6];
				float m28 = matrix2[k][j+7];
				result[pos] += m1 * m21;
				result[pos+1] += m1 * m22;
				result[pos+2] += m1 * m23;
				result[pos+3] += m1 * m24;
				result[pos+4] += m1 * m25;
				result[pos+5] += m1 * m26;
				result[pos+6] += m1 * m27;
				result[pos+7] += m1 * m28;
			}

			for (;j < MATRIX_SIZE; j++, pos++) {
				float m2 = matrix2[k][j];
				result[pos] += m1 * m2;
			}
		}
	}
	// printf("Child %d::%d finished\n", id, getpid());
	exit(0);
}

void init_workers(float **matrix1, float **matrix2, float *result) {
	pid_t pid;
	for (int i = 0; i < maxCores; i++) {
		pid = fork();

		if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
			child_worker(matrix1, matrix2, result, i);
		}
	}
}

void wait_workers() {
	for (int i = 0; i < maxCores; i++) {
		wait(NULL);
	}
}

void validate_mult(float **matrix1, float **matrix2, float *result) {
	if (!IS_VALIDATE_MATRIX || print_mode || MATRIX_SIZE > VALIDATE_MAX_SIZE) {
		return;
	}
	float **verify = init_matrix(MATRIX_SIZE, 0);
	size_t c_pos;
	int end = MATRIX_SIZE/8;
	for (int i = 0; i < MATRIX_SIZE; i++) {
		float *r = verify[i];
		int k = 0;
		for (; k < end; k+= 8) {
			size_t pos = 0;
			float m1 = matrix1[i][k] + matrix1[i][k+1] + matrix1[i][k+2] + matrix1[i][k+3] + matrix1[i][k+4] + matrix1[i][k+5] + matrix1[i][k+6] + matrix1[i][k+7];
			int j = 0;
			for (; j < end; j+=8, pos+=8) {
				float m21 = matrix2[k][j];
				float m22 = matrix2[k][j+1];
				float m23 = matrix2[k][j+2];
				float m24 = matrix2[k][j+3];
				float m25 = matrix2[k][j+4];
				float m26 = matrix2[k][j+5];
				float m27 = matrix2[k][j+6];
				float m28 = matrix2[k][j+7];
				r[pos] += m1 * m21;
				r[pos+1] += m1 * m22;
				r[pos+2] += m1 * m23;
				r[pos+3] += m1 * m24;
				r[pos+4] += m1 * m25;
				r[pos+5] += m1 * m26;
				r[pos+6] += m1 * m27;
				r[pos+7] += m1 * m28;
			}

			for (; j < MATRIX_SIZE; j++, pos++) {
				float m2 = matrix2[k][j];
				r[pos] += m1 * m2;
			}
		}

		for (; k < MATRIX_SIZE; k++) {
			size_t pos = 0;
			float m1 = matrix1[i][k];
			int j = 0;
			for (; j < end; j+=8, pos+=8) {
				float m21 = matrix2[k][j];
				float m22 = matrix2[k][j+1];
				float m23 = matrix2[k][j+2];
				float m24 = matrix2[k][j+3];
				float m25 = matrix2[k][j+4];
				float m26 = matrix2[k][j+5];
				float m27 = matrix2[k][j+6];
				float m28 = matrix2[k][j+7];
				r[pos] += m1 * m21;
				r[pos+1] += m1 * m22;
				r[pos+2] += m1 * m23;
				r[pos+3] += m1 * m24;
				r[pos+4] += m1 * m25;
				r[pos+5] += m1 * m26;
				r[pos+6] += m1 * m27;
				r[pos+7] += m1 * m28;
			}

			for (;j < MATRIX_SIZE; j++, pos++) {
				float m2 = matrix2[k][j];
				r[pos] += m1 * m2;
			}
		}
		c_pos = i * MATRIX_SIZE;
		for (int j = 0; j < MATRIX_SIZE; j++, c_pos++) {
			if (fabs(verify[i][j] - result[c_pos]) >= 0.0001) {
				printf("Validation failed at %d %d\n", i, j);
				printf("Expected: %f, Actual: %f\n", verify[i][j], result[c_pos]);
				exit(EXIT_FAILURE);
			}
		}
	}
	printf("Validation passed\n");
	print_matrix(verify, MATRIX_SIZE, "Actual Matrix");
	free_matrix(verify, MATRIX_SIZE);
}

float *create_shm_matrix(int* shmid) {
    size_t size = sizeof(float) * MATRIX_SIZE * MATRIX_SIZE;
    *shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
    
	if (*shmid == -1) {
        perror("shmget data");
        exit(EXIT_FAILURE);
    }
    float *result = (float*)shmat(*shmid, NULL, 0);

    if (result == (void *)-1) {
        perror("shmat data");
        exit(EXIT_FAILURE);
    }
    memset(result, 0, size);
    return result;
}

int main(int argc, char const *argv[]) {
	srand(time(NULL));
	validate_args(argc, argv);
	int res_id;
	double time_taken = 0;
	struct timeval begin, end;
	float **matrix1 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix1, MATRIX_SIZE, "Matrix 1");
	float **matrix2 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix2, MATRIX_SIZE, "Matrix 2");
	float *result = create_shm_matrix(&res_id);

	// time taken to perform matrix multiplication related tasks
	gettimeofday(&begin, NULL);
	init_workers(matrix1, matrix2, result);
	wait_workers();
	print_result(result, MATRIX_SIZE, "Result");
	gettimeofday(&end, NULL);
	time_taken = getdetlatimeofday(&begin, &end);
	print_stats(time_taken);

	// validate the result and free the memory
	gettimeofday(&begin, NULL);
	validate_mult(matrix1, matrix2, result);
	gettimeofday(&end, NULL);
	free_matrix(matrix1, MATRIX_SIZE);
	free_matrix(matrix2, MATRIX_SIZE);
    shmdt(result);
    shmctl(res_id, IPC_RMID, NULL);
	return 0;
}


