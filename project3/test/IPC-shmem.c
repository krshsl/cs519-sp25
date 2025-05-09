/* CS 519, Spring 2025: Project 1 - Part 2
 * IPC using shared memory to perform matrix multiplication.
 * Feel free to extend or change any code or functions below.
 *
 * use following command to get approx performance stats:
 * make && /usr/bin/time -f "%e,%U,%S" -q ./shmem_1.o -p
 * make && /usr/bin/time -f "%e,%U,%S" -q ./shmem_2.o -p
 * make && /usr/bin/time -f "%e,%U,%S" -q ./shmem_3.o -p
 *
 * Please make sure to run the following commands to make the program work!
 * echo "#kernel shmmni increased for shmem implementation" | sudo tee -a /etc/sysctl.conf
 * echo "kernel.shmmni=16384" | sudo tee -a /etc/sysctl.conf
 * tail -n 2 /etc/sysctl.conf
 * sudo sysctl -p
 */
#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef sys_set_inactive_pid
#define sys_set_inactive_pid 336
#endif

//Add all your global variables and definitions here.
#define MATRIX_SIZE 4096
// floating point precision issues if value exceeds certain thresholds
#define MAX_VALUE (MATRIX_SIZE < 1024 ? 2048 : \
				 (MATRIX_SIZE < 4096 ? 1024 : \
				 (MATRIX_SIZE <= 8192 ? 512 : 4)))
#define MAX_CORES 96
#define MAX_ALLOWED_CORES 16
#define IS_VALIDATE_MATRIX 0
#define VALIDATE_MAX_SIZE 10000
#define PRINT_CAP 4

#define MINF(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAXF(X, Y) (((X) > (Y)) ? (X) : (Y))

struct active_time_shd {
    pthread_mutex_t mutex;
    unsigned long long proc_time;
    int counter;
};

int maxCores = MINF(MATRIX_SIZE, MAX_CORES); // hard limit to not size of matrix
int maxAllowedCores = -1;
int print_mode = 1; // print in csv format for easy parsing
int active_time_id = -1;
int res_id;
int shm_ids[MATRIX_SIZE];
struct active_time_shd *active_time = NULL;
pthread_mutexattr_t attr;

void validate_args(int argc, char const **argv);
double getdetlatimeofday(struct timeval *begin, struct timeval *end);
void print_stats(double time_taken);

void init_active_time(void);
float random_num(float min, float max);
float **init_matrix(int size, unsigned char is_zero);
float *create_shm_rows(int *shmid);
float **create_shm_matrix(void);
void free_active_time(void);
void free_shm_matrix(float **matrix);
void print_matrix(float **matrix, int size, const char *msg);
void print_result(float **matrix, int size, const char *msg);
void free_matrix(float **matrix, int size);
pid_t *init_workers(float **matrix1, float **matrix2, float **result);
void child_worker(float **matrix1, float **matrix2, float **result, int id);
void wait_workers(pid_t *children);
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

	maxAllowedCores = MINF(sysconf(_SC_NPROCESSORS_ONLN), MAX_ALLOWED_CORES);
}

/* Time function that calculates time between start and end */
double getdetlatimeofday(struct timeval *begin, struct timeval *end) {
    return (end->tv_sec + end->tv_usec * 1.0 / 1000000) -
           (begin->tv_sec + begin->tv_usec * 1.0 / 1000000);
}

/* Time function that calculates time between start and end as precisely as possible */
long long elapsed_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
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

void init_active_time(void) {
    active_time_id = shmget(IPC_PRIVATE, sizeof(struct active_time_shd), IPC_CREAT | 0666);
    if (active_time_id < 0) {
        perror("shmget");
        exit(1);
    }

    active_time = (struct active_time_shd *)shmat(active_time_id, NULL, 0);
    if (active_time == (void *)-1) {
        perror("shmat");
        exit(1);
    }
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&active_time->mutex, &attr);
    active_time->proc_time = 0LL;
    active_time->counter = 1;
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

void print_result(float **matrix, int size, const char *msg) {
	if (size > PRINT_CAP) {
		return;
	}

	printf("%s\n", msg);
	for (int i = 0; i < size; i++) {
		float *row = matrix[i];
		for (int j = 0; j < size; j++) {
			printf("%f\t", row[j]);
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

void child_worker(float **matrix1, float **matrix2, float **result, int id) {
	// printf("Child %d::%d started\n", id, getpid());
	struct timespec start_time, end_time;
	int end = MATRIX_SIZE/8;
	int local_count = 0;
	for (int i = id; i < MATRIX_SIZE; i+=maxCores) {
		float *r = result[i];
		int k = 0;

		clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
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
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

#ifdef DO_YIELD
        sched_yield();
#endif

		if (local_count > 2) {
		    continue; // micro opt
		}

		pthread_mutex_lock(&active_time->mutex);
		active_time->proc_time = MAXF(active_time->proc_time, elapsed_ns(start_time, end_time));
		active_time->counter++;
		pthread_mutex_unlock(&active_time->mutex);
		local_count++;
	}
	// printf("Child %d::%d finished\n", id, getpid());
	exit(0);
}

pid_t *init_workers(float **matrix1, float **matrix2, float **result) {
	pid_t *children = calloc(maxCores, sizeof(pid_t));
    pid_t pid;
    cpu_set_t set;
    CPU_ZERO(&set);
	for (int i = 0, j = 0; i < maxCores; i++, j = (j+1)%maxAllowedCores) {
		pid = fork();

		if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            CPU_SET(j, &set);
            if (sched_setaffinity(getpid(), sizeof(set), &set) == -1)
                err(EXIT_FAILURE, "sched_setaffinity");

			child_worker(matrix1, matrix2, result, i);
		}
		children[i] = pid;
	}
	return children;
}

void wait_workers(pid_t *children) {
    while(active_time->counter < 2*maxCores) {
        sched_yield();
    }

#ifdef DO_INACTIVE
    for (int i = 0, j = 0; i < maxCores; i++, j = (j+1)%maxAllowedCores) {
        if (children[i]) {
            if (syscall(sys_set_inactive_pid, children[i], j, active_time->proc_time) < 0) {
                perror("syscall set_inactive (test)");
                syscall(sys_set_inactive_pid); // just to be safe....
                exit(EXIT_FAILURE);
            }
        }
    }
#endif

	for (int i = 0; i < maxCores; i++) {
		wait(NULL);
	}
}

void validate_mult(float **matrix1, float **matrix2, float **result) {
	if (!IS_VALIDATE_MATRIX || print_mode || MATRIX_SIZE > VALIDATE_MAX_SIZE) {
		return;
	}
	float **verify = init_matrix(MATRIX_SIZE, 0);
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

		for (int j = 0; j < MATRIX_SIZE; j++) {
			if (fabs(verify[i][j] - result[i][j]) >= 0.0001) {
				printf("Validation failed at %d %d\n", i, j);
				printf("Expected: %f, Actual: %f\n", verify[i][j], result[i][j]);
				exit(EXIT_FAILURE);
			}
		}
	}
	printf("Validation passed\n");
	print_matrix(verify, MATRIX_SIZE, "Actual Matrix");
	free_matrix(verify, MATRIX_SIZE);
}

float *create_shm_rows(int *shmid) {
	size_t size = sizeof(float) * MATRIX_SIZE;
	*shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);

	if (*shmid == -1) {
		perror("shmget data");
		return NULL;
	}
	float *result = (float*)shmat(*shmid, NULL, 0);

	if (result == (void *)-1) {
		perror("shmat data");
	    shmctl(*shmid, IPC_RMID, NULL);
		return NULL;
	}
	memset(result, 0, size);
	return result;
}

float **create_shm_matrix() {
    memset(shm_ids, -1, sizeof(float) * MATRIX_SIZE);

    size_t size = sizeof(float*) * MATRIX_SIZE;
    res_id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);

	if (res_id == -1) {
        perror("shmget data");
        exit(EXIT_FAILURE);
    }
	float **result = (float**)shmat(res_id, NULL, 0);

	if (result == (void *)-1) {
        perror("shmat data");
	    shmctl(res_id, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

	for (int i = 0; i < MATRIX_SIZE; i++) {
		shm_ids[i] = -1;
		result[i] = create_shm_rows(&shm_ids[i]);
		if (result[i] == NULL || shm_ids[i] == -1) {
		    free_shm_matrix(result);
			exit(EXIT_FAILURE);
		}
	}
    return result;
}

void free_shm_matrix(float **matrix) {
	for (int i = 0; i < MATRIX_SIZE; i++) {
		if (shm_ids[i] != -1) {
			shmdt(matrix[i]);
			shmctl(shm_ids[i], IPC_RMID, NULL);
		}
	}

	if (res_id != -1) {
		shmdt(matrix);
		shmctl(res_id, IPC_RMID, NULL);
	}
}

void free_active_time(void) {
    pthread_mutex_destroy(&active_time->mutex);
    pthread_mutexattr_destroy(&attr);
    shmdt(active_time);
    shmctl(active_time_id, IPC_RMID, NULL);
}

int main(int argc, char const *argv[]) {
	srand(time(NULL));
	validate_args(argc, argv);
    pid_t *children = NULL;
	double time_taken = 0;
	struct timeval begin, end;
	float **matrix1 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix1, MATRIX_SIZE, "Matrix 1");
	float **matrix2 = init_matrix(MATRIX_SIZE, 1);
	print_matrix(matrix2, MATRIX_SIZE, "Matrix 2");
	float **result = create_shm_matrix();
	init_active_time();

	// time taken to perform matrix multiplication related tasks
	gettimeofday(&begin, NULL);
	children = init_workers(matrix1, matrix2, result);
	wait_workers(children);
	print_result(result, MATRIX_SIZE, "Result");
	gettimeofday(&end, NULL);
	time_taken = getdetlatimeofday(&begin, &end);
	print_stats(time_taken);

	// validate the result and free the memory
	gettimeofday(&begin, NULL);
	validate_mult(matrix1, matrix2, result);
	gettimeofday(&end, NULL);
	free(children);
	free_matrix(matrix1, MATRIX_SIZE);
	free_matrix(matrix2, MATRIX_SIZE);
    free_shm_matrix(result);
    free_active_time();
	return 0;
}
