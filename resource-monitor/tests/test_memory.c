#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    void *ptr;
    size_t size;
} allocation_t;

static void usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [--target-mb MB] [--step-mb MB] [--sleep-ms MS] [--hold-seconds S]\n",
            prog);
}

int main(int argc, char **argv) {
    size_t target_mb = 256;
    size_t step_mb = 8;
    int sleep_ms = 200;
    int hold_seconds = 5;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--target-mb") == 0 && i + 1 < argc) {
            target_mb = (size_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--step-mb") == 0 && i + 1 < argc) {
            step_mb = (size_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--sleep-ms") == 0 && i + 1 < argc) {
            sleep_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--hold-seconds") == 0 && i + 1 < argc) {
            hold_seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (target_mb == 0 || step_mb == 0) {
        fprintf(stderr, "Os valores de target-mb e step-mb devem ser positivos.\n");
        return 1;
    }

    size_t target_bytes = target_mb * 1024UL * 1024UL;
    size_t step_bytes = step_mb * 1024UL * 1024UL;
    size_t capacity = target_bytes / step_bytes + 2;

    allocation_t *allocations = calloc(capacity, sizeof(allocation_t));
    if (allocations == NULL) {
        perror("calloc");
        return 1;
    }

    size_t allocated_bytes = 0;
    size_t chunks = 0;

    while (allocated_bytes < target_bytes) {
        void *buffer = malloc(step_bytes);
        if (buffer == NULL) {
            perror("malloc");
            break;
        }
        memset(buffer, (int)(chunks & 0xFF), step_bytes);

        allocations[chunks].ptr = buffer;
        allocations[chunks].size = step_bytes;

        allocated_bytes += step_bytes;
        chunks++;

        if (sleep_ms > 0) {
            usleep((useconds_t)sleep_ms * 1000);
        }
    }

    double allocated_mb = (double)allocated_bytes / (1024.0 * 1024.0);
    printf("memory_test target_mb=%zu allocated_mb=%.2f chunks=%zu sleep_ms=%d\n",
           target_mb, allocated_mb, chunks, sleep_ms);
    fflush(stdout);

    if (hold_seconds > 0) {
        printf("Mantendo alocações por %d segundo(s)...\n", hold_seconds);
        fflush(stdout);
        sleep(hold_seconds);
    }

    for (size_t i = 0; i < chunks; i++) {
        free(allocations[i].ptr);
    }
    free(allocations);
    return 0;
}
