// CPU intensive workload used to validate profiler/cgroup experiments.
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    unsigned long long iterations;
} worker_result_t;

static struct timespec deadline;

static int still_running(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec > deadline.tv_sec) {
        return 0;
    }
    if (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec) {
        return 0;
    }
    return 1;
}

static void *cpu_worker(void *arg) {
    worker_result_t *result = (worker_result_t *)arg;
    unsigned long long local_iters = 0;
    double value = 1.000001;

    while (still_running()) {
        for (int i = 0; i < 1000; i++) {
            value += value * 0.000001;
            value -= 0.0000005;
        }
        local_iters += 1000;
    }

    result->iterations = local_iters;
    return NULL;
}

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s [--duration SEG] [--threads N]\n", prog);
}

int main(int argc, char **argv) {
    int duration = 15;
    int threads = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (duration <= 0 || threads <= 0) {
        fprintf(stderr, "Parâmetros inválidos: duration=%d threads=%d\n", duration, threads);
        return 1;
    }

    worker_result_t *results = calloc((size_t)threads, sizeof(worker_result_t));
    pthread_t *tids = calloc((size_t)threads, sizeof(pthread_t));
    if (results == NULL || tids == NULL) {
        perror("calloc");
        free(results);
        free(tids);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += duration;

    for (int i = 0; i < threads; i++) {
        if (pthread_create(&tids[i], NULL, cpu_worker, &results[i]) != 0) {
            fprintf(stderr, "Falha ao criar thread %d: %s\n", i, strerror(errno));
            threads = i;
            break;
        }
    }

    unsigned long long total_iterations = 0;
    for (int i = 0; i < threads; i++) {
        pthread_join(tids[i], NULL);
        total_iterations += results[i].iterations;
    }

    free(results);
    free(tids);

    double throughput = (double)total_iterations / duration;
    printf("cpu_test duration=%d threads=%d total_iterations=%llu throughput=%.2f iter/s\n",
           duration, threads, total_iterations, throughput);
    fflush(stdout);

    return 0;
}
